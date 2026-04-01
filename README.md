# Qoraal
### Embedded Application Framework
#### Zephyr | FreeRTOS | ThreadX | POSIX

## What is Qoraal?

Qoraal is a small framework for building embedded applications.

It sits above the RTOS and gives you a cleaner way to organize services, events, persistence, shell tooling, logging, and local web UI, while still keeping the underlying platform visible.

The short version is simple:
Qoraal helps you build embedded applications, not just firmware modules.

## Why it exists

Most embedded projects do not get messy because the low-level code is impossible.
They get messy because the application layer slowly spreads out across callbacks, globals, init ordering, storage glue, network glue, and one-off patterns.

Qoraal came out of trying to keep that part of the system under control.

The main ideas behind it are:

- Events should be explicit.
- Services should have a lifecycle.
- Portability should be designed in early.
- Common plumbing should be wrapped once and reused.
- The framework should stay small enough that it does not fight the application.

It is not trying to be clever.
It is trying to keep a growing embedded system understandable.

## What Qoraal gives you

### Services with lifecycle

Qoraal gives you a service model with clear ownership:

- init / start / stop
- dependency ordering
- failure handling
- a cleaner application structure above raw tasks

This is one of the parts that makes a project feel more like an application and less like a collection of subsystems.

### Event-driven application flow

Qoraal leans toward explicit event flow instead of hidden control paths:

- deferred work
- timed work
- service-to-service signaling
- structured transitions in higher-level application logic

### OS abstraction

Qoraal has a portability layer for the common things most embedded applications need:

- threads and priorities
- mutexes, semaphores, and events
- timers and time conversion
- basic context helpers where supported

The point is not to hide the OS completely.
The point is to stop rewriting the same application-facing glue over and over.

### Logging and shell support

Qoraal includes a set of practical diagnostics tools:

- channel-based logging
- deferred logging
- pluggable log sinks
- a lightweight shell for bring-up, diagnostics, and configuration

### Persistence and filesystem access

Qoraal includes simple abstractions that are useful in real products:

- registry-style persistent configuration
- portable filesystem access
- patterns that work well for settings, content, and UI-backed state

### Embedded HTTP and local UI

Qoraal can also serve local device UI:

- HTTP handlers
- page generation
- integration with application state
- integration with persistence

This is one of the areas where it becomes more than a utility layer. It gives you a practical way to build a real device-facing application interface in C.

## Why not just use the RTOS directly?

You still do.

Qoraal does not replace Zephyr, FreeRTOS, ThreadX, or POSIX.
It sits above them and gives the application layer more structure.

Raw RTOS primitives are necessary, but they are not the same thing as an application model.
Once a device has services, persistent configuration, network behavior, a shell, a local UI, and product logic, most teams end up building some version of this layer for themselves anyway.

Qoraal is that layer, just made explicit and reusable.

## Where it fits well

Qoraal is a good fit for:

- connected embedded products
- devices with a local web UI
- systems with multiple application services
- firmware that needs persistent configuration
- applications where state and lifecycle matter
- projects that need to run across RTOS and POSIX environments

It is probably not the right fit for:

- very small single-purpose firmware
- projects that only need a few threads and drivers
- teams that do not want any framework conventions at all

## Example projects

### Device-style applications

The examples in this repository and related projects show the intended style:

- services with lifecycle control
- shell-backed diagnostics
- registry-backed configuration
- filesystem-backed content
- embedded web UI

### Qoraal Tic-Tac-Toe

[qoraal-tictactoe](https://github.com/navaro/qoraal-tictactoe) is a good example of what Qoraal can do when pushed further.

It shows:

- dynamic HTML generation driven by hierarchical state machines
- HTTP requests translated into application events
- structured rendering instead of a pile of strings and callbacks
- normal C functions for the domain logic

It is a more ambitious example than a simple config page, and that is exactly why it is useful. It shows that Qoraal is not just a toolbox. It can support real application structure.

## Supported environments

### Verified

- Zephyr
- FreeRTOS
- ThreadX / Azure RTOS
- POSIX (Linux, macOS, Windows/MinGW)

### Legacy

- ChibiOS

If you run Qoraal on another target and it works well, open an issue or PR with the RTOS, board, toolchain, and port notes.

## Main components

### OS abstraction layer

Portable API for:

- threads and priorities
- mutexes, semaphores, and events
- timers and time conversion
- basic context helpers

### Service task system

A lightweight task engine with:

- priority task queues
- deferred work from interrupts
- timed and delayed tasks
- optional waitable tasks
- an internal worker model tuned for embedded systems

### Service management

Define services with a clear lifecycle and explicit ownership.

### Logging

- channel-based logging per module
- deferred logging using tasks
- pluggable sinks such as UART, RTT, POSIX stdout, Zephyr logging, and FLASH

### Shell

A small shell that works well for:

- bring-up
- diagnostics
- configuration
- test and automation hooks

### Watchdog management

A unified model for watchdog integration across targets.

## Quick start

### POSIX demo

1. Build:

```bash
make all
```

2. When the Qoraal shell starts, run:

```bash
. runme.sh
```

You should see the demo services start and the application come up in a normal host environment.

### Using Qoraal with Zephyr

```cmake
add_subdirectory(qoraal)
target_link_libraries(app PRIVATE qoraal)
```

Then enable the relevant port and sample configuration for your target.

## Why people tend to like working with it

- It adds structure without becoming heavy.
- It stays close to C and the platform.
- It works well for real product code.
- It scales from simple device pages to more stateful application behavior.

Qoraal is meant to sit quietly in the middle of an embedded system and make the application easier to build and maintain.
