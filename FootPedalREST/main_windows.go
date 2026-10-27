//go:build windows

package main

import (
	"fmt"
	"log"
	"net/url"
	"runtime"
)

func main() {
	runtime.LockOSThread() // Win32 message queues belong to an OS thread.
	defer runtime.UnlockOSThread()

	log.SetFlags(log.Ldate | log.Ltime | log.Lmicroseconds)
	fmt.Println("FootPedalREST v2 - USB HID keyboard pedal to independent REST actions")
	fmt.Println("---------------------------------------------------------------")

	configPath, err := settingsPath()
	if err != nil {
		log.Fatalf("Find settings path: %v", err)
	}
	cfg, created, err := loadSettings(configPath)
	if err != nil {
		log.Fatalf("Read settings: %v", err)
	}
	if err = validateSettings(cfg); err != nil {
		log.Fatalf("Invalid settings.json: %v", err)
	}
	if created {
		log.Printf("Created default settings file: %s", configPath)
		log.Print("Edit settings.json and restart to configure each of the six REST requests.")
	}

	log.Printf("Settings file: %s", configPath)
	log.Print("Configured REST actions (query parameter values hidden):")
	for _, key := range pedalKeys {
		for _, edge := range pedalEdges {
			action := cfg.Requests[key][edge]
			u, _ := url.Parse(action.URL) // validated above
			if u.RawQuery != "" {
				u.RawQuery = "REDACTED"
			}
			if u.User != nil {
				u.User = url.User("REDACTED")
			}
			log.Printf("  %-2s %-7s -> %-6s %s", key, edge, normalizedMethod(action.Method), u.String())
		}
	}
	if cfg.DeviceFilter == "" {
		log.Print("Device filter: EMPTY (ALL keyboards can trigger F1/F2/F3)")
	} else {
		log.Printf("Device filter: %s", cfg.DeviceFilter)
	}
	log.Print("Ready: listening for F1, F2 and F3 press + release events.")
	log.Print("The program receives input even while another window is active.")
	log.Print("Press Ctrl+C in this console to stop.")

	listener := newRawListener(cfg)
	if err := listener.runWindowsLoop(); err != nil {
		log.Fatalf("Start Windows raw-input listener: %v", err)
	}
	log.Print("Stopped.")
}
