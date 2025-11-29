# Qoraal
### Embedded Application Framework  
#### Zephyr • FreeRTOS • ThreadX • POSIX • ChibiOS  

---

## What is Qoraal?

Qoraal is a **flexible, event‑driven application framework** for embedded systems — structured where it matters, fluid where it helps.

It gives you a portable OS abstraction, a scalable task system, clean service management, logging, shell support, and watchdog handling — all designed to keep embedded applications modular and sane across multiple RTOSes and POSIX environments.

---

## Supported Environments

- **Zephyr**
- **FreeRTOS**
- **ThreadX / Azure RTOS**
- **ChibiOS**
- **POSIX (Linux, macOS, Windows/MinGW)**

Write once. Build anywhere.

---

## Main Components

### **OS Abstraction Layer**
Portable API for:
- Threads & priorities  
- Mutexes, semaphores, events  
- Timers and time conversion  
- Basic IRQ/context detection (where supported)

This is what allows Qoraal services to run unchanged on different OSes.

---

### **Service Task System**
A lightweight task engine with:
- Prioritized task queues  
- Deferred work from interrupts  
- Timed / delayed tasks  
- Optional waitable tasks  
- Internal thread pool tuned for embedded work

Perfect for systems that need responsiveness without chaos.

---

### **Service Management**
Define services with clear lifecycles:

- init → start → stop  
- dependency ordering  
- failure handling and restart logic  

A simple structure that prevents big‑app entropy.

---

### **Logging**
- Channel‑based logging per module  
- Pluggable backends (RTT, UART, POSIX stdout, Zephyr logging)  
- Optimized for low‑resource systems  

---

### **Shell**
A small, terminal‑independent command shell with:
- Commands
- Arguments
- Simple scripting
- Easy routing to UART, RTT, TCP, or Zephyr console

Great for bring‑up and debugging.

---

### **Watchdog Management**
Unified interface for hardware watchdogs.

Feed from services or tasks → avoid random device resets.

---

## Quick Start (POSIX Demo)

1. Open a Codespace or your local environment.
2. Run:

   **Windows**
   ```
   build_and_run.bat
   ```

   **Linux/macOS**
   ```
   ./build_and_run.sh
   ```

3. When the shell appears:
   ```bash
   . runme.sh
   ```

You’ll see the demo services start up.

---

## Using Qoraal with Zephyr

### 1. Add Qoraal to Your Project

```cmake
add_subdirectory(qoraal)
target_link_libraries(app PRIVATE qoraal)
```

### 2. Enable Zephyr Port

```cmake
target_compile_definitions(qoraal PRIVATE CFG_OS_ZEPHYR)
```

### 3. Minimal `prj.conf`

```conf
CONFIG_MAIN_STACK_SIZE=4096
CONFIG_THREAD_STACK_INFO=y
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_CONSOLE=y
CONFIG_SHELL=y
CONFIG_UART_CONSOLE=y
```

### 4. Initialize Qoraal

```c
#include "qoraal/qoraal.h"
#include "qoraal/services.h"

int main(void)
{
    qoraal_init();
    // Register & start your services
    for (;;) k_sleep(K_MSEC(1000));
}
```

---

## Why Qoraal?

- Clean structure without rigidity  
- Portable across the major RTOS ecosystems  
- Designed for real products, not toy demos  
- Small, sharp, predictable  

Qoraal sits quietly in the center of your system and keeps everything running smoothly, without ever getting in your way.
