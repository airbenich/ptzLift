package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"sync"
	"testing"
)

func sampleEvent() PedalEvent {
	return PedalEvent{
		ID: "test-event-123", Key: "F2", Event: "release",
		Timestamp: "2026-10-22T10:30:40Z",
		Device:    `\\?\HID#VID_1234&PID_ABCD#sample`,
	}
}

type receivedRequest struct {
	Method        string
	Path          string
	Query         string
	Body          string
	ContentType   string
	Action        string
	Shared        string
	Authorization string
	Idempotency   string
}

func TestSixIndependentRequestsAndBodies(t *testing.T) {
	var mu sync.Mutex
	var calls []receivedRequest
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		mu.Lock()
		calls = append(calls, receivedRequest{
			Method: r.Method, Path: r.URL.Path, Query: r.URL.RawQuery,
			Body: string(body), ContentType: r.Header.Get("Content-Type"),
			Action: r.Header.Get("X-Action"), Shared: r.Header.Get("X-Shared"),
			Authorization: r.Header.Get("Authorization"),
			Idempotency:   r.Header.Get("Idempotency-Key"),
		})
		mu.Unlock()
		w.WriteHeader(http.StatusCreated)
	}))
	defer srv.Close()

	s := defaultSettings()
	s.BearerToken = "global-token"
	s.Headers = map[string]string{"X-Shared": "global"}
	method := map[string]map[string]string{
		"F1": {"press": "POST", "release": "PUT"},
		"F2": {"press": "GET", "release": "DELETE"},
		"F3": {"press": "PATCH", "release": "POST"},
	}
	bodies := map[string]string{
		"F1/press":   `{"state":"on"}`,
		"F1/release": `{"state":"off"}`,
		"F2/release": `{"delete":true}`,
		"F3/press":   `{"level":42}`,
	}

	for _, key := range pedalKeys {
		for _, edge := range pedalEdges {
			name := key + "/" + edge
			action := RequestSettings{
				URL:     srv.URL + "/commands/" + strings.ToLower(name) + "?constant=yes",
				Method:  method[key][edge],
				Headers: map[string]string{"X-Action": name},
			}
			if payload, ok := bodies[name]; ok {
				action.Body = json.RawMessage(payload)
			}
			if name == "F1/press" {
				action.Headers["Authorization"] = "Bearer action-token"
				action.Headers["X-Shared"] = "action-override"
			}
			s.Requests[key][edge] = action
		}
	}
	if err := validateSettings(s); err != nil {
		t.Fatal(err)
	}

	for _, key := range pedalKeys {
		for _, edge := range pedalEdges {
			e := sampleEvent()
			e.Key, e.Event, e.ID = key, edge, "event-id-"+key+"-"+edge
			status, err := sendEvent(srv.Client(), s, e)
			if err != nil || status != http.StatusCreated {
				t.Fatalf("send %s/%s: status=%d error=%v", key, edge, status, err)
			}
		}
	}
	mu.Lock()
	defer mu.Unlock()
	if len(calls) != 6 {
		t.Fatalf("expected 6 unique HTTP requests, got %d", len(calls))
	}
	i := 0
	for _, key := range pedalKeys {
		for _, edge := range pedalEdges {
			name := key + "/" + edge
			got := calls[i]
			i++
			if got.Method != method[key][edge] || got.Path != "/commands/"+strings.ToLower(name) || got.Query != "constant=yes" {
				t.Errorf("%s route: got %s %s?%s", name, got.Method, got.Path, got.Query)
			}
			if got.Body != bodies[name] {
				t.Errorf("%s body: got %q, want %q", name, got.Body, bodies[name])
			}
			if _, exists := bodies[name]; exists && got.ContentType != "application/json" {
				t.Errorf("%s content-type=%q", name, got.ContentType)
			}
			if _, exists := bodies[name]; !exists && got.ContentType != "" {
				t.Errorf("%s expected no Content-Type, got %q", name, got.ContentType)
			}
			if got.Idempotency != "event-id-"+key+"-"+edge || got.Action != name {
				t.Errorf("%s headers: %+v", name, got)
			}
			if name == "F1/press" {
				if got.Authorization != "Bearer action-token" || got.Shared != "action-override" {
					t.Errorf("per-action header override failed: %+v", got)
				}
			} else if got.Authorization != "Bearer global-token" || got.Shared != "global" {
				t.Errorf("global headers failed: %+v", got)
			}
		}
	}
}

func TestNoImplicitDataInBodyOrURL(t *testing.T) {
	s := defaultSettings()
	e := sampleEvent()
	req, err := buildRequest(s, e)
	if err != nil {
		t.Fatal(err)
	}
	if req.Method != "POST" || req.URL.String() != "http://127.0.0.1:8080/f2/release" || req.Body != nil {
		t.Fatalf("unexpected request: method=%s url=%s body=%v", req.Method, req.URL, req.Body)
	}
	if req.Header.Get("Idempotency-Key") != e.ID {
		t.Fatal("missing unique idempotency key")
	}
}

func TestDefaultSettingsCreateAndLoad(t *testing.T) {
	path := filepath.Join(t.TempDir(), "settings.json")
	cfg, created, err := loadSettings(path)
	if err != nil || !created {
		t.Fatalf("loadSettings create=%t err=%v", created, err)
	}
	if err := validateSettings(cfg); err != nil {
		t.Fatalf("invalid defaults: %v", err)
	}
	seen := map[string]bool{}
	for _, key := range pedalKeys {
		for _, edge := range pedalEdges {
			action := cfg.Requests[key][edge]
			if action.Method != "POST" || seen[action.URL] || action.URL == "" {
				t.Fatalf("each default route must have distinct URL: %s %s %+v", key, edge, action)
			}
			seen[action.URL] = true
		}
	}
	if len(seen) != 6 {
		t.Fatalf("expected 6 URLs, got %d", len(seen))
	}
	if _, err := os.Stat(path); err != nil {
		t.Fatal(err)
	}
	again, created, err := loadSettings(path)
	if err != nil || created || !reflect.DeepEqual(again, cfg) {
		t.Fatalf("reload created=%t err=%v cfg=%+v", created, err, again)
	}
}

func TestLegacySettingsRejected(t *testing.T) {
	path := filepath.Join(t.TempDir(), "settings.json")
	if err := os.WriteFile(path, []byte(`{"endpoint":"http://127.0.0.1:8080/pedal","method":"POST","timeout_seconds":5}`), 0600); err != nil {
		t.Fatal(err)
	}
	_, _, err := loadSettings(path)
	if err == nil || !strings.Contains(err.Error(), "v1 settings") {
		t.Errorf("old config must give actionable message, got %v", err)
	}
}

func TestValidateAllSixAndRejectTypos(t *testing.T) {
	tests := []struct {
		name   string
		modify func(*Settings)
		want   string
	}{
		{"missing F1", func(s *Settings) { delete(s.Requests, "F1") }, "requests.F1"},
		{"missing release", func(s *Settings) { delete(s.Requests["F3"], "release") }, "requests.F3.release"},
		{"unexpected key", func(s *Settings) { s.Requests["F4"] = s.Requests["F3"] }, "unknown requests key"},
		{"unexpected edge", func(s *Settings) { s.Requests["F3"]["up"] = s.Requests["F3"]["press"] }, "unknown requests.F3 event"},
		{"invalid URL", func(s *Settings) { a := s.Requests["F1"]["press"]; a.URL = "not-a-url"; s.Requests["F1"]["press"] = a }, "requests.F1.press.url"},
		{"invalid method", func(s *Settings) { a := s.Requests["F1"]["press"]; a.Method = "TRACE"; s.Requests["F1"]["press"] = a }, "requests.F1.press.method"},
		{"GET with body", func(s *Settings) {
			a := s.Requests["F2"]["release"]
			a.Method = "GET"
			a.Body = json.RawMessage(`{"x":2}`)
			s.Requests["F2"]["release"] = a
		}, "body must be omitted"},
		{"invalid JSON body", func(s *Settings) {
			a := s.Requests["F2"]["release"]
			a.Body = json.RawMessage(`{bad`)
			s.Requests["F2"]["release"] = a
		}, "body must be valid JSON"},
		{"invalid global header", func(s *Settings) { s.Headers["Bad Header"] = "x" }, "invalid HTTP header"},
		{"invalid action header", func(s *Settings) {
			a := s.Requests["F2"]["press"]
			a.Headers = map[string]string{"X-Test": "value\nother"}
			s.Requests["F2"]["press"] = a
		}, "cannot contain a newline"},
		{"invalid timeout", func(s *Settings) { s.TimeoutSeconds = 0 }, "timeout_seconds"},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := defaultSettings()
			tt.modify(&s)
			if err := validateSettings(s); err == nil || !strings.Contains(err.Error(), tt.want) {
				t.Errorf("expected %s, got %v", tt.want, err)
			}
		})
	}
}

func TestUnknownEventIsNotSent(t *testing.T) {
	s := defaultSettings()
	for _, e := range []PedalEvent{{Key: "F4", Event: "press"}, {Key: "F1", Event: "held"}} {
		if _, err := buildRequest(s, e); err == nil {
			t.Fatalf("expected error for %+v", e)
		}
	}
}

func TestExplicitRawBodyAndMethodNormalization(t *testing.T) {
	s := defaultSettings()
	a := s.Requests["F1"]["press"]
	a.Method = " put "
	a.Body = json.RawMessage(`{"array":[1,2],"yes":true}`)
	s.Requests["F1"]["press"] = a
	if err := validateSettings(s); err != nil {
		t.Fatal(err)
	}
	req, err := buildRequest(s, PedalEvent{Key: "F1", Event: "press", ID: "unique"})
	if err != nil {
		t.Fatal(err)
	}
	if req.Method != "PUT" {
		t.Fatalf("got %q", req.Method)
	}
	body, _ := io.ReadAll(req.Body)
	if string(body) != string(a.Body) {
		t.Fatalf("got body %q", body)
	}
	if req.ContentLength != int64(len(a.Body)) {
		t.Fatalf("content-length %d", req.ContentLength)
	}
}

func TestNoAutomaticRetryOnFailure(t *testing.T) {
	n := 0
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		n++
		w.WriteHeader(http.StatusServiceUnavailable)
	}))
	defer srv.Close()
	s := defaultSettings()
	a := s.Requests["F1"]["press"]
	a.URL = srv.URL
	s.Requests["F1"]["press"] = a
	status, err := sendEvent(newHTTPClient(s), s, PedalEvent{Key: "F1", Event: "press", ID: "once"})
	if status != 503 || err == nil || n != 1 {
		t.Fatal(fmt.Sprintf("status=%d err=%v count=%d", status, err, n))
	}
}

func TestRedirectIsNotFollowed(t *testing.T) {
	count := 0
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		count++
		if r.URL.Path == "/original" {
			http.Redirect(w, r, "/redirected", http.StatusTemporaryRedirect)
			return
		}
		t.Error("HTTP client followed an unexpected redirect")
	}))
	defer srv.Close()
	s := defaultSettings()
	a := s.Requests["F1"]["press"]
	a.URL = srv.URL + "/original"
	s.Requests["F1"]["press"] = a
	status, err := sendEvent(newHTTPClient(s), s, PedalEvent{Key: "F1", Event: "press", ID: "once"})
	if err == nil || status != http.StatusTemporaryRedirect || count != 1 {
		t.Fatalf("expected a single failed 307 request, got HTTP %d / err=%v / calls=%d", status, err, count)
	}
}
