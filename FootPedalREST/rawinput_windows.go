//go:build windows

package main

import (
	"fmt"
	"log"
	"os"
	"os/signal"
	"runtime"
	"strings"
	"syscall"
	"time"
	"unsafe"
)

const (
	wmInput             = 0x00FF
	wmInputDeviceChange = 0x00FE
	wmClose             = 0x0010
	wmDestroy           = 0x0002
	ridInput            = 0x10000003
	ridiDeviceName      = 0x20000007
	rimTypeKeyboard     = 1
	riKeyBreak          = 0x0001
	ridevInputSink      = 0x00000100
	ridevDeviceNotify   = 0x00002000
	gidcRemoval         = 2
	vkF1                = 0x70
	vkF2                = 0x71
	vkF3                = 0x72
	queueCapacity       = 1024
)

var (
	dllUser32                   = syscall.NewLazyDLL("user32.dll")
	dllKernel32                 = syscall.NewLazyDLL("kernel32.dll")
	procRegisterClassExW        = dllUser32.NewProc("RegisterClassExW")
	procCreateWindowExW         = dllUser32.NewProc("CreateWindowExW")
	procDefWindowProcW          = dllUser32.NewProc("DefWindowProcW")
	procDestroyWindow           = dllUser32.NewProc("DestroyWindow")
	procPostQuitMessage         = dllUser32.NewProc("PostQuitMessage")
	procPostMessageW            = dllUser32.NewProc("PostMessageW")
	procGetMessageW             = dllUser32.NewProc("GetMessageW")
	procTranslateMessage        = dllUser32.NewProc("TranslateMessage")
	procDispatchMessageW        = dllUser32.NewProc("DispatchMessageW")
	procRegisterRawInputDevices = dllUser32.NewProc("RegisterRawInputDevices")
	procGetRawInputData         = dllUser32.NewProc("GetRawInputData")
	procGetRawInputDeviceInfoW  = dllUser32.NewProc("GetRawInputDeviceInfoW")
	procGetModuleHandleW        = dllKernel32.NewProc("GetModuleHandleW")
	activeListener              *rawListener
	callbackPtr                 = syscall.NewCallback(windowProcedure)
)

type winClassEx struct {
	Size        uint32
	Style       uint32
	WndProc     uintptr
	ClassExtra  int32
	WindowExtra int32
	Instance    uintptr
	Icon        uintptr
	Cursor      uintptr
	Background  uintptr
	MenuName    uintptr
	ClassName   uintptr
	IconSmall   uintptr
}

type winPoint struct {
	X int32
	Y int32
}

type winMessage struct {
	Window  uintptr
	Message uint32
	_       uint32 // x64 MSG padding between UINT and WPARAM
	WParam  uintptr
	LParam  uintptr
	Time    uint32
	Point   winPoint
	Private uint32
}

type rawInputDevice struct {
	UsagePage uint16
	Usage     uint16
	Flags     uint32
	Target    uintptr
}

type rawInputHeader struct {
	Type   uint32
	Size   uint32
	Device uintptr
	WParam uintptr
}

type rawKeyboard struct {
	MakeCode  uint16
	Flags     uint16
	Reserved  uint16
	VKey      uint16
	Message   uint32
	ExtraInfo uint32
}

type heldKey struct {
	Device uintptr
	Key    uint16
}

type rawListener struct {
	cfg       Settings
	window    uintptr
	events    chan PedalEvent
	done      chan struct{}
	held      map[heldKey]bool
	nameCache map[uintptr]string
}

func newRawListener(cfg Settings) *rawListener {
	l := &rawListener{
		cfg:       cfg,
		events:    make(chan PedalEvent, queueCapacity),
		done:      make(chan struct{}),
		held:      make(map[heldKey]bool),
		nameCache: make(map[uintptr]string),
	}
	go l.httpWorker()
	return l
}

func (l *rawListener) httpWorker() {
	defer close(l.done)
	client := newHTTPClient(l.cfg)
	for e := range l.events {
		status, err := sendEvent(client, l.cfg, e)
		if err != nil {
			log.Printf("REST FAIL %s %s id=%s: %v", e.Key, e.Event, e.ID, err)
		} else {
			log.Printf("REST OK   %s %s id=%s HTTP %d", e.Key, e.Event, e.ID, status)
		}
	}
}

func (l *rawListener) enqueue(e PedalEvent) {
	select {
	case l.events <- e:
		log.Printf("INPUT     %s %s from %s", e.Key, e.Event, e.Device)
	default:
		log.Printf("QUEUE FULL - lost %s %s; destination is too slow", e.Key, e.Event)
	}
}

func (l *rawListener) handleRawInput(inputHandle uintptr) {
	headerSize := uint32(unsafe.Sizeof(rawInputHeader{}))
	keyboardSize := uint32(unsafe.Sizeof(rawKeyboard{}))
	var required uint32

	result, _, callErr := procGetRawInputData.Call(
		inputHandle, ridInput, 0,
		uintptr(unsafe.Pointer(&required)), uintptr(headerSize),
	)
	if uint32(result) == ^uint32(0) || required < headerSize+keyboardSize {
		log.Printf("GetRawInputData size query failed: %v", callErr)
		return
	}

	data := make([]byte, int(required))
	result, _, callErr = procGetRawInputData.Call(
		inputHandle, ridInput,
		uintptr(unsafe.Pointer(&data[0])),
		uintptr(unsafe.Pointer(&required)), uintptr(headerSize),
	)
	if uint32(result) == ^uint32(0) || uint32(result) < headerSize+keyboardSize {
		log.Printf("GetRawInputData read failed: %v", callErr)
		return
	}

	header := (*rawInputHeader)(unsafe.Pointer(&data[0]))
	if header.Type != rimTypeKeyboard {
		return
	}
	keyboard := (*rawKeyboard)(unsafe.Pointer(&data[int(headerSize)]))

	var keyName string
	switch keyboard.VKey {
	case vkF1:
		keyName = "F1"
	case vkF2:
		keyName = "F2"
	case vkF3:
		keyName = "F3"
	default:
		return
	}

	deviceName := l.deviceName(header.Device)
	if l.cfg.DeviceFilter != "" && !strings.Contains(
		strings.ToUpper(deviceName), strings.ToUpper(l.cfg.DeviceFilter),
	) {
		return
	}

	pressed := keyboard.Flags&riKeyBreak == 0
	held := heldKey{Device: header.Device, Key: keyboard.VKey}
	eventName := "release"
	if pressed {
		if l.held[held] {
			return // Windows auto-repeat while the pedal remains down.
		}
		l.held[held] = true
		eventName = "press"
	} else {
		delete(l.held, held)
		// Release is still sent if the program started while the key was held.
	}

	l.enqueue(PedalEvent{
		ID:        makeEventID(),
		Key:       keyName,
		Event:     eventName,
		Timestamp: time.Now().UTC().Format(time.RFC3339Nano),
		Device:    deviceName,
	})
}

func (l *rawListener) deviceName(device uintptr) string {
	if name, found := l.nameCache[device]; found {
		return name
	}
	if device == 0 {
		return "(unidentified keyboard)"
	}

	var charCount uint32
	result, _, _ := procGetRawInputDeviceInfoW.Call(
		device, ridiDeviceName, 0, uintptr(unsafe.Pointer(&charCount)),
	)
	if uint32(result) == ^uint32(0) || charCount == 0 || charCount > 32768 {
		return "(device path unavailable)"
	}
	chars := make([]uint16, int(charCount))
	result, _, _ = procGetRawInputDeviceInfoW.Call(
		device, ridiDeviceName,
		uintptr(unsafe.Pointer(&chars[0])),
		uintptr(unsafe.Pointer(&charCount)),
	)
	if uint32(result) == ^uint32(0) {
		return "(device path unavailable)"
	}
	name := syscall.UTF16ToString(chars)
	l.nameCache[device] = name
	return name
}

func (l *rawListener) removeDevice(device uintptr) {
	delete(l.nameCache, device)
	for h := range l.held {
		if h.Device == device {
			delete(l.held, h)
		}
	}
}

func windowProcedure(window, message, wParam, lParam uintptr) uintptr {
	switch uint32(message) {
	case wmInput:
		if activeListener != nil {
			activeListener.handleRawInput(lParam)
		}
		// Always pass WM_INPUT to DefWindowProc for Windows foreground cleanup.
	case wmInputDeviceChange:
		if wParam == gidcRemoval && activeListener != nil {
			activeListener.removeDevice(lParam)
		}
	case wmClose:
		procDestroyWindow.Call(window)
		return 0
	case wmDestroy:
		procPostQuitMessage.Call(0)
		return 0
	}
	result, _, _ := procDefWindowProcW.Call(window, message, wParam, lParam)
	return result
}

func (l *rawListener) runWindowsLoop() error {
	activeListener = l
	defer func() {
		activeListener = nil
		close(l.events)
		log.Print("Finishing queued REST requests (up to 15 seconds)...")
		select {
		case <-l.done:
			log.Print("All queued events processed.")
		case <-time.After(15 * time.Second):
			log.Print("Timed out during shutdown; unsent queued requests may be lost.")
		}
	}()

	instance, _, err := procGetModuleHandleW.Call(0)
	if instance == 0 {
		return fmt.Errorf("GetModuleHandleW: %w", err)
	}
	className, err := syscall.UTF16PtrFromString("FootPedalREST.RawInput.Window")
	if err != nil {
		return err
	}
	windowClass := winClassEx{
		Size:      uint32(unsafe.Sizeof(winClassEx{})),
		WndProc:   callbackPtr,
		Instance:  instance,
		ClassName: uintptr(unsafe.Pointer(className)),
	}
	atom, _, callErr := procRegisterClassExW.Call(uintptr(unsafe.Pointer(&windowClass)))
	runtime.KeepAlive(className)
	runtime.KeepAlive(windowClass)
	if atom == 0 {
		return fmt.Errorf("RegisterClassExW: %w", callErr)
	}

	// HWND_MESSAGE is (HWND)-3: a window which receives messages but has
	// no visible UI. RIDEV_INPUTSINK lets it receive background keyboard input.
	const hwndMessage uintptr = ^uintptr(2)
	window, _, callErr := procCreateWindowExW.Call(
		0, uintptr(unsafe.Pointer(className)), 0, 0,
		0, 0, 0, 0, hwndMessage, 0, instance, 0,
	)
	runtime.KeepAlive(className)
	if window == 0 {
		return fmt.Errorf("CreateWindowExW: %w", callErr)
	}
	l.window = window

	device := rawInputDevice{
		UsagePage: 0x01, // Generic Desktop
		Usage:     0x06, // Keyboard
		Flags:     ridevInputSink | ridevDeviceNotify,
		Target:    window,
	}
	success, _, callErr := procRegisterRawInputDevices.Call(
		uintptr(unsafe.Pointer(&device)), 1,
		unsafe.Sizeof(device),
	)
	runtime.KeepAlive(device)
	if success == 0 {
		procDestroyWindow.Call(window)
		return fmt.Errorf("RegisterRawInputDevices: %w", callErr)
	}

	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt)
	stopSignal := make(chan struct{})
	go func() {
		select {
		case <-sig:
			procPostMessageW.Call(window, wmClose, 0, 0)
		case <-stopSignal:
		}
	}()
	defer func() {
		close(stopSignal)
		signal.Stop(sig)
	}()

	for {
		var msg winMessage
		ret, _, callErr := procGetMessageW.Call(
			uintptr(unsafe.Pointer(&msg)), 0, 0, 0,
		)
		if int32(ret) == -1 {
			return fmt.Errorf("GetMessageW: %w", callErr)
		}
		if ret == 0 {
			return nil // WM_QUIT
		}
		procTranslateMessage.Call(uintptr(unsafe.Pointer(&msg)))
		procDispatchMessageW.Call(uintptr(unsafe.Pointer(&msg)))
	}
}
