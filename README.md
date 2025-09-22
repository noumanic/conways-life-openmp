# Conway's Game of Life (OpenMP + SDL2)

**Description:**
Conway's Game of Life implemented in **C** with **OpenMP parallelization** and **SDL2 graphical visualization**.
Includes performance analysis of serial vs. parallel (static & guided scheduling, with/without critical sections) and an interactive graphical mode with real-time controls.

---

## 📂 File Structure

```
.
├── game_of_life_text.c    # Text-based implementation with OpenMP performance analysis
├── game_of_life_visual.c  # SDL2 graphical implementation with parallel/serial toggle
├── TODO.txt               # Project requirements & tasks
└── README.md              # Project documentation
```

---

## ⚡ Requirements

* **Compiler**: GCC with OpenMP support
* **Libraries**: SDL2

On Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install gcc libsdl2-dev
```

---

## 🚀 Compilation

### 1. Text-based version

```bash
gcc -fopenmp game_of_life_text.c -o game_of_life_text
```

### 2. Graphical SDL2 version

```bash
gcc -fopenmp game_of_life_visual.c -o game_of_life_visual -lSDL2
```

---

## ▶️ Running the Programs

### 1. Text-based

```bash
./game_of_life_text
```

* Runs all versions (serial, static, guided, with/without critical section)
* Prints average times, speedups, and final grid

### 2. Graphical

```bash
./game_of_life_visual
```

Options:

* `-p` → Start in parallel mode
* `-r [density]` → Start with random initialization (default 0.3 density)
* `-g` → Start with glider pattern
* `-h` → Show help menu
* `-n` → Disable stats overlay

Example:

```bash
./game_of_life_visual -p -r 0.5
```

---

## 🎮 Controls (Graphical Version)

* `ESC` → Exit simulation
* `P` → Toggle parallel/serial
* `SPACE` → Pause for 3 seconds
* `R` → Reset grid with random pattern
* `S` → Toggle stats overlay

---

## 📊 Output

* **Text version**: prints performance analysis + final grid
* **Graphical version**: interactive window with visualization and optional stats overlay

---

## ✨ Author

Developed by **Muhammad Nouman Hafeez** (@noumanic)