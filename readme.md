# Team Auto-Chess
## Important specs:
* Tile size: 1.75 inches
* Piece diameter: 0.75 inches
* Steps/inch X: 127.93
* Steps/inch Y: 114.48

## Wire requirements
15 wires total

BOT SYSTEM - 6 wires
2 wires for x motor
2 wires for y motor
1 wire shared between motors
1 wire for magnet

HUMAN SYSTEM - 4 wires
~~3 wires for power addressing~~
~~3 wires for poll addressing~~
^^3 wire for 6 pin shift register
1 wire for polling

UI SYSTEM - 5 wires
2 wires for display
3 wires for knob


### ESP32 Chess Bot: Serialized Master Pinout

| System | Component | ESP32 GPIO | Direction | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **Bot** | Stepper X-STEP | **2** | Output | Horizontal Pulse |
| (6 Wires) | Stepper Y-STEP | **32** | Output | Vertical Pulse |
|          | Stepper X-DIR  | **4** | Output | Horizontal Direction |
|          | Stepper Y-DIR  | **33** | Output | Vertical Direction |
|          | Shared Enable  | **14** | Output | Motor On/Off |
|          | Magnet GATE    | **13** | Output | MOSFET Trigger |
| **Matrix** | **SR Data** | **25** | Output | Drives Rows AND Mux Address |
| (4 Wires) | **SR Clock** | **26** | Output | Ticks the Shift Register |
|          | **SR Latch** | **27** | Output | Updates the Board State |
|          | **Matrix Read**| **34** | **Input**| The "Found Piece" Signal |
| **UI** | LCD SDA / SCL | **21 / 22**| I2C    | Move Display |
| (5 Wires) | Knob CLK / DT | **35 / 36**| **Input**| Menu Rotation |
|          | Knob Button   | **39** | **Input**| Selection |

### ⚠️ Final Hardware Warnings


2. **Current Draw:** When the Magnet and both Steppers are active, the current draw will be high. Use a separate **12V power supply** for them and join the **GND** to the ESP32 GND.
3. **The Diode Rule:** You must have 64 diodes in your matrix (one per square) or the magnetic fields/reed switches will cause "Ghosting" where one piece looks like it's in multiple places.