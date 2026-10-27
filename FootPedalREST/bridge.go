package main

import (
	"bytes"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"time"
)

var pedalKeys = []string{"F1", "F2", "F3"}
var pedalEdges = []string{"press", "release"}

// RequestSettings defines the complete HTTP action for one key transition.
// Body, when present, is sent as-is as JSON. There is NO implicit event body.
type RequestSettings struct {
	URL     string            `json:"url"`
	Method  string            `json:"method"`
	Headers map[string]string `json:"headers,omitempty"`
	Body    json.RawMessage   `json:"body,omitempty"`
}

// Requests contains independent press and release actions for F1/F2/F3.
// Global headers are inherited by each request; action headers can override.
type Settings struct {
	DeviceFilter   string                                `json:"device_filter"`
	BearerToken    string                                `json:"bearer_token"`
	Headers        map[string]string                     `json:"headers"`
	TimeoutSeconds int                                   `json:"timeout_seconds"`
	Requests       map[string]map[string]RequestSettings `json:"requests"`
}

type PedalEvent struct {
	ID        string `json:"id"`
	Key       string `json:"key"`
	Event     string `json:"event"`
	Timestamp string `json:"timestamp"`
	Device    string `json:"device"`
}

func defaultSettings() Settings {
	s := Settings{
		DeviceFilter:   "",
		BearerToken:    "",
		Headers:        map[string]string{},
		TimeoutSeconds: 5,
		Requests:       make(map[string]map[string]RequestSettings),
	}
	for _, key := range pedalKeys {
		s.Requests[key] = make(map[string]RequestSettings)
		for _, edge := range pedalEdges {
			s.Requests[key][edge] = RequestSettings{
				URL:    fmt.Sprintf("http://127.0.0.1:8080/%s/%s", strings.ToLower(key), edge),
				Method: http.MethodPost,
			}
		}
	}
	return s
}

func settingsPath() (string, error) {
	executable, err := os.Executable()
	if err != nil {
		return "", err
	}
	return filepath.Join(filepath.Dir(executable), "settings.json"), nil
}

func loadSettings(path string) (Settings, bool, error) {
	b, err := os.ReadFile(path)
	if errors.Is(err, os.ErrNotExist) {
		s := defaultSettings()
		data, e := json.MarshalIndent(s, "", "  ")
		if e != nil {
			return Settings{}, false, e
		}
		if e = os.WriteFile(path, append(data, '\n'), 0600); e != nil {
			return Settings{}, false, fmt.Errorf("create %s: %w", path, e)
		}
		return s, true, nil
	}
	if err != nil {
		return Settings{}, false, err
	}

	// A legacy settings file is not silently converted. The old single URL
	// cannot express the six independent destinations requested in v2.
	var fields map[string]json.RawMessage
	if err := json.Unmarshal(b, &fields); err != nil {
		return Settings{}, false, fmt.Errorf("parse %s: %w", path, err)
	}
	if _, old := fields["endpoint"]; old {
		return Settings{}, false, fmt.Errorf("%s is a v1 settings file with a single endpoint; use the new 'requests' format (see README.md) and configure all six URLs", path)
	}

	var s Settings
	decoder := json.NewDecoder(bytes.NewReader(b))
	decoder.DisallowUnknownFields() // catch misspelled actions/options
	if err := decoder.Decode(&s); err != nil {
		return Settings{}, false, fmt.Errorf("parse %s: %w", path, err)
	}
	if s.Headers == nil {
		s.Headers = map[string]string{}
	}
	return s, false, nil
}

func normalizedMethod(method string) string {
	method = strings.ToUpper(strings.TrimSpace(method))
	if method == "" {
		return http.MethodPost
	}
	return method
}

func validHeaderName(name string) bool {
	if name == "" {
		return false
	}
	for i := 0; i < len(name); i++ {
		c := name[i]
		if (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strings.ContainsRune("!#$%&'*+-.^_`|~", rune(c)) {
			continue
		}
		return false
	}
	return true
}

func validateHeaders(headers map[string]string, location string) error {
	for k, v := range headers {
		if !validHeaderName(k) {
			return fmt.Errorf("%s contains an invalid HTTP header name %q", location, k)
		}
		if strings.ContainsAny(v, "\r\n") {
			return fmt.Errorf("%s header %q cannot contain a newline", location, k)
		}
	}
	return nil
}

func validateURL(address string) error {
	u, err := url.Parse(address)
	if err != nil || u == nil || u.Host == "" || (u.Scheme != "http" && u.Scheme != "https") {
		return fmt.Errorf("must be an absolute http:// or https:// URL (got %q)", address)
	}
	if u.Fragment != "" {
		return errors.New("must not contain a URL fragment (#...)")
	}
	return nil
}

func validateSettings(s Settings) error {
	if s.TimeoutSeconds < 1 || s.TimeoutSeconds > 120 {
		return fmt.Errorf("timeout_seconds must be between 1 and 120 (got %d)", s.TimeoutSeconds)
	}
	if err := validateHeaders(s.Headers, "headers"); err != nil {
		return err
	}
	for key := range s.Requests {
		if key != "F1" && key != "F2" && key != "F3" {
			return fmt.Errorf("unknown requests key %q; only F1, F2, F3 are supported", key)
		}
	}
	for _, key := range pedalKeys {
		events, exists := s.Requests[key]
		if !exists {
			return fmt.Errorf("requests.%s is missing; define press and release", key)
		}
		for edge := range events {
			if edge != "press" && edge != "release" {
				return fmt.Errorf("unknown requests.%s event %q; use press or release", key, edge)
			}
		}
		for _, edge := range pedalEdges {
			action, exists := events[edge]
			location := fmt.Sprintf("requests.%s.%s", key, edge)
			if !exists {
				return fmt.Errorf("%s is missing", location)
			}
			if err := validateURL(action.URL); err != nil {
				return fmt.Errorf("%s.url %w", location, err)
			}
			method := normalizedMethod(action.Method)
			switch method {
			case http.MethodGet, http.MethodPost, http.MethodPut, http.MethodPatch, http.MethodDelete:
			default:
				return fmt.Errorf("%s.method must be GET, POST, PUT, PATCH or DELETE (got %q)", location, action.Method)
			}
			if method == http.MethodGet && len(action.Body) != 0 {
				return fmt.Errorf("%s.body must be omitted for GET requests", location)
			}
			if len(action.Body) != 0 && !json.Valid(action.Body) {
				return fmt.Errorf("%s.body must be valid JSON", location)
			}
			if err := validateHeaders(action.Headers, location+".headers"); err != nil {
				return err
			}
		}
	}
	return nil
}

func makeEventID() string {
	var b [16]byte
	if _, err := rand.Read(b[:]); err == nil {
		return hex.EncodeToString(b[:])
	}
	return fmt.Sprintf("fallback-%d", time.Now().UnixNano())
}

func requestForEvent(s Settings, e PedalEvent) (RequestSettings, error) {
	events, exists := s.Requests[e.Key]
	if !exists {
		return RequestSettings{}, fmt.Errorf("no requests configured for key %q", e.Key)
	}
	action, exists := events[e.Event]
	if !exists {
		return RequestSettings{}, fmt.Errorf("no %s request configured for key %q", e.Event, e.Key)
	}
	return action, nil
}

// buildRequest sends each transition to its own configured URL/method.
// No key/event metadata is implicitly added to URL, query, or request body.
// The Idempotency-Key header remains available to recognize duplicate events.
func buildRequest(s Settings, e PedalEvent) (*http.Request, error) {
	action, err := requestForEvent(s, e)
	if err != nil {
		return nil, err
	}
	method := normalizedMethod(action.Method)
	var body io.Reader
	if len(action.Body) != 0 {
		body = bytes.NewReader(action.Body)
	}
	req, err := http.NewRequest(method, action.URL, body)
	if err != nil {
		return nil, err
	}
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	req.Header.Set("Idempotency-Key", e.ID)
	if strings.TrimSpace(s.BearerToken) != "" {
		req.Header.Set("Authorization", "Bearer "+strings.TrimSpace(s.BearerToken))
	}
	for k, v := range s.Headers {
		req.Header.Set(k, v)
	}
	for k, v := range action.Headers { // action-specific overrides global
		req.Header.Set(k, v)
	}
	return req, nil
}

func sendEvent(client *http.Client, s Settings, e PedalEvent) (int, error) {
	req, err := buildRequest(s, e)
	if err != nil {
		return 0, err
	}
	response, err := client.Do(req)
	if err != nil {
		return 0, err
	}
	defer response.Body.Close()
	_, _ = io.Copy(io.Discard, io.LimitReader(response.Body, 64*1024))
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		return response.StatusCode, fmt.Errorf("HTTP %s", response.Status)
	}
	return response.StatusCode, nil
}

func newHTTPClient(s Settings) *http.Client {
	return &http.Client{
		Timeout: time.Duration(s.TimeoutSeconds) * time.Second,
		// Do not turn one pedal edge into multiple HTTP calls via redirects.
		CheckRedirect: func(_ *http.Request, _ []*http.Request) error {
			return http.ErrUseLastResponse
		},
	}
}
