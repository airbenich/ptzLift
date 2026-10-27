package main

import (
	"fmt"
	"io"
	"log"
	"net/http"
)

func main() {
	// Match all test endpoints, including /f1/press, /f1/release, and so on.
	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		defer r.Body.Close()
		body, _ := io.ReadAll(io.LimitReader(r.Body, 65536))
		log.Printf("%s %s from %s", r.Method, r.URL.RequestURI(), r.RemoteAddr)
		if len(body) > 0 {
			log.Printf("BODY: %s", body)
		}
		if key := r.Header.Get("Idempotency-Key"); key != "" {
			log.Printf("ID: %s", key)
		}
		w.Header().Set("Content-Type", "application/json")
		_, _ = fmt.Fprint(w, `{"ok":true}`)
	})

	log.Println("REST Test Receiver - listening on http://127.0.0.1:8080/")
	log.Println("Start FootPedalREST.exe and press/release F1, F2 or F3.")
	log.Println("Each of the six actions should appear as a distinct request URL.")
	log.Println("Ctrl+C to stop. This test server only listens on localhost.")
	log.Fatal(http.ListenAndServe("127.0.0.1:8080", nil))
}
