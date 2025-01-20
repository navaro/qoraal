# Qoraal
### Embedded Application Framework

---

## Overview

**Leveled vs. Layered Programming**—that is the question.

- **Leveled programming**: Focuses on hierarchical abstractions, where each "level" has a specific and singular purpose.
- **Layered programming**: Embraces flexibility and organic design, with components interacting freely. Layers represent logical groupings rather than rigid hierarchies.

### What’s Qoraal About?

Qoraal embodies **layered architecture**, where modules coexist side by side, allowing for a seamless and flexible development experience.

So lets get into it!



---

## Quick Start

1. Open a Codespace or your development environment.
2. Run the following commands to build and execute Qoraal:
   ```
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```
   > :bulb: On Wondows try: ```cmake -G "MinGW Makefiles" ..```
   > :bulb: To do a clean build, start with: ``` cmake --build . --target clean ``` or, if you want to check out qoraal again: ```rm -rf _deps```

3. in the shell that will open:
   ```bash
   . runme.sh

> Keep your scanners peeled—there’s more to come.
