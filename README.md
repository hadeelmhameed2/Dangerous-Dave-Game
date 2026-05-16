# 🚀 Dangerous Dave: The Box2D Evolution

Welcome to the physics-backed remake of Dangerous Dave! This project features a fully functional stage, custom ECS logic, and a dynamic graphical user interface.

### 🖼️ Visual Asset Mapping

* **Dave (The Player):** A dynamic Box2D physics body equipped with a state-safe ground contact counter.
* **Green Digits :** Decoded frame-by-frame to render the player's live score directly onto the screen space.
* **The Door :** Handles state transitions, cleanly swapping textures from locked to unlocked when the trophy objective is met.

### 🎮 Controls

* `A` / `D` (or Left/Right Arrows) – Move Left/Right
* `W` / `Space` – Jump (Gated: only available when touching a platform!)
* `Esc` – Quick Exit

### 📂 Repository Structure

* `lib/` – Vendored upstream Box2D source code.
* `res/` – Graphical assets, HUD strips, tiles, and character sprites.
* `bagel.h` – The core custom ECS framework driving the game loop.

### ⚙️ Quick Start & Build Instructions

1. Open the project root folder in **CLion**.
2. Verify that the `lib/box2d` source tree and the `res/` asset directory are present.
3. Click **Reload CMake Project** and run the main target.
