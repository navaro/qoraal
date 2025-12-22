# Qoraal
### Embedded Application Framework  
#### Zephyr • FreeRTOS • ThreadX • POSIX  

---

## What is Qoraal?

Qoraal is a **flexible, event-driven application framework** for embedded systems — structured where it matters, fluid where it helps.

It gives you a portable OS abstraction, a scalable task system, clean service management, logging, shell support, and watchdog handling, designed to keep embedded applications modular and sane across multiple RTOSes or POSIX environments.

---

## Philosophy

Embedded projects don’t usually fail because someone cant write a driver.  
They fail slowly, from entropy.

Qoraal is built around a few simple beliefs:

- **Events over spaghetti:** make "what happened" explicit.
- **Services with lifecycles:** init/start/stop beats "some global init order."
- **Portable by design:** the OS port is a boundary, not an afterthought.
- **Small, predictable primitives:** boring on purpose but reliable in production.
- **No framework religion:** you can adopt it piece-by-piece and keep your own style.

Qoraal's goal is not to be clever, it’s to keep your system **coherent** as it grows.

---

## Supported environments

### Verified (actively tested)
- **Zephyr**
- **FreeRTOS**
- **ThreadX / Azure RTOS**
- **POSIX** (Linux, macOS, Windows/MinGW)

### Legacy (previously supported)
- **ChibiOS** 

> "Write once. Build anywhere." — within the boundaries above.  
> If you run Qoraal on a new target and it holds up, open an issue/PR with the RTOS, toolchain, and board/host details.

---

## Main components

### OS abstraction layer
Portable API for:
- Threads & priorities  
- Mutexes, semaphores, events  
- Timers and time conversion  
- Basic IRQ/context detection (where supported)

This is what allows Qoraal services to run unchanged on different OSes.

---

### Service task system
A lightweight task engine with:
- Priority task queues 
- Deferred work from interrupts  
- Timed / delayed tasks  
- Optional waitable tasks  
- Internal thread pool tuned for embedded work

Perfect for systems that need responsiveness without chaos.

---

### Service management
Define services with clear lifecycles:

- init → start → stop  
- dependency ordering  
- failure handling and restart logic  

A simple structure that prevents big-app entropy.

---

### Logging
- Channel-based logging per module
- Deferred logging using tasks
- Pluggable log channels (RTT, UART, POSIX stdout, Zephyr logging, FLASH)  
- Optimized for low-resource systems  

---

### Shell
A small, terminal-independent command shell with:
- Commands
- Arguments
- Simple scripting
- Easy routing to UART, RTT, TCP, or Zephyr console

Great for bring-up and debugging or configuration management.

---

### Watchdog management
Unified interface for hardware watchdogs.

Feed from tasks → avoid random device resets.

---

## Quick start (POSIX demo)

1. Open a Codespace or your local environment.
2. Run:

   **Windows/Linux/macOS/GitHub Codespace**
   ```bash
   make all
   ```

3. When the Qoraal shell starts:
   ```bash
   . runme.sh
   ```

You’ll see the demo services start up.

---

## Using Qoraal with Zephyr

### 1) Add Qoraal to your project
```cmake
add_subdirectory(qoraal)
target_link_libraries(app PRIVATE qoraal)
```

### 2) Enable Zephyr port

Check the sample test project. Should run on most platforms.

---

## Why Qoraal?

- Clean structure without rigidity  
- Portable across major RTOS ecosystems  
- Designed for real products, not toy demos  
- Small, sharp, predictable  

Qoraal sits quietly in the center of your system and keeps everything coherent, without getting in your way.
