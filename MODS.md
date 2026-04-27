# LARCset Mods Tracker

## Status Key
- `[done]` — implemented
- `[wip]` — in progress
- `[planned]` — designed, not yet built
- `[idea]` — discussed, not yet designed

---

## 1. ESP32 + Si5351 VFO `[wip]`
Replace the analog crystal-controlled VFO with ESP32 + Si5351.

- Si5351 CLK0 → LPF (pi, 100nH air-wound + 2x 47pF) → 100nF coupling cap → L1 hot end (pin 1) on LARCset
- Si5351 CLK2 → LPF → 100nF → BFO injection point (T1/T3 product detector winding)
- Disable original VFO oscillator transistor (lift collector or cut trace at L1 pin 1 node)
- Firmware: `firmware/vfo/vfo.ino`
  - WiFi AP: "LARCset-VFO" (open, no password)
  - Web UI at http://192.168.4.1 — virtual tuning knob, step control
  - TFT display: ST7735 1.44" 128x128, abacus-style frequency display
  - 40m LSB, 7.000–7.300 MHz, starts at 7.055 MHz
  - High-side injection: VFO = IF (11.059 MHz) + RF freq

**Pinout:**
```
GPIO 21 (SDA) → Si5351 SDA
GPIO 22 (SCL) → Si5351 SCL
GPIO 18 (SCK) → TFT SCK
GPIO 23 (MOSI)→ TFT SDA/MOSI
GPIO 5        → TFT CS
GPIO 4        → TFT DC / A0
GPIO 13       → TFT RST
GPIO 33       → PTT sense (LOW = TX)
```

**TODO:**
- [ ] Verify injection point on physical board (L1 vs post-L1 node)
- [ ] Calibrate Si5351 correction value against WWV or SDR
- [ ] Tune BFO_FREQ (11057500 Hz) by ear for best LSB voice
- [ ] Re-add encoder when Dupont cables available (GPIO 25/26/27)

---

## 2. Adjustable Power Output `[planned]`
Keep PA linear while varying output power.

- **Primary:** MCP4131 digital pot (I2C) on driver stage input — reduces drive to PA
  - PA (IRF510) stays biased in Class AB at all times
  - I2C controllable from ESP32
- **Secondary:** Gate bias trimpot set once at build time for proper Class AB
- **Future:** ALC loop — directional coupler samples forward power → ESP32 ADC → closes loop via MCP4131

---

## 3. Modular Architecture `[idea]`
Eurorack-inspired swappable modules on a common bus.

**Bus (IDC header per module):**
- Power: +12V, +5V, +3.3V, GND
- RF: SMA in/out per module (50Ω)
- Control: I2C (ESP32 master, modules self-identify)
- Audio: TX_AUDIO, RX_AUDIO
- Signals: PTT, KEY, AGC_VOLTAGE, BAND[0..2]
- Meters: FWD_PWR, REF_PWR

**Module list:**
- VFO module (ESP32 + Si5351 + TFT) — this build
- LPF bank module (relay-switched per band, I2C controlled)
- BPF / front-end module
- PA module (IRF510, swappable power levels)
- T/R switch module (relay or PIN diode)
- Meter module (VU meters — see mod #4)
- Audio DSP module (CW filter, NR, compression)
- Keyer module (iambic + sidetone)
- Digital interface module (USB audio + CAT)
- 2m transverter module (see mod #6)

**Form factor:** Eurorack 3U, 84HP rack, 3D printed rails + blanks

---

## 4. VU Meter Module `[idea]`
Use existing VU meter movements in a dedicated panel module.

**Meter 1 — S-meter (RX):**
- Tap AGC voltage from IF chain
- Op-amp scaling circuit → meter movement
- Useful range: S1–S9+40dB

**Meter 2 — Power/SWR (TX):**
- Directional coupler on PA output (wound toroid, ~20 min build)
- Forward + reflected voltages → peak detectors → two meter movements
- Toggle switch: RX = S-meter, TX = power out
- Read SWR at a glance, satisfying needle swing on TX

---

## 5. Mixer Balance Fix `[planned]`
The 1N4148 diode ring relies on T2 transformer symmetry for LO suppression. Any winding
imbalance causes LO leakthrough at the IF port, degrading sensitivity and transmit spectral
purity. Three options, easiest first:

### Option A — Trimmer cap null `[planned]`
Add a 5–30 pF ceramic trimmer across one half of T2's LO secondary (center tap to one end).
Adjusting it compensates phase/amplitude imbalance in the winding and nulls LO feedthrough
without any board rework beyond adding two solder points.

- Adjust with SDR dongle (or second receiver) on IF port: tune for minimum LO signal
- Frequency-dependent but stable across 40m; set once at 7.100 MHz
- Cost: ~$1, no new ICs, no layout changes

### Option B — Diode matching `[planned]`
Select four 1N4148 diodes with matched forward voltage. Unequal Vf breaks ring symmetry
and causes both LO and RF feedthrough.

- Measure Vf with DMM in diode mode; aim for <2 mV spread across all four
- Drop-in replacement, no other changes

### Option C — ADE-1 / SBL-1 module replacement `[idea]`
Replace the entire ring + T1/T2/T3 with a Mini-Circuits doubly-balanced mixer module.

- ~10 dB noise floor improvement
- Better dynamic range / IMD
- Drop-in if footprint sized correctly
- ADE-1 rated to 500 MHz (same part as Mod #6 transverter — buy a pair)

---

## 6. 2m Transverter Module `[idea]`
Extend the LARCset to 144 MHz SSB using the existing IF strip.

**Architecture:**
```
2m antenna → VHF BPF → ADE-1 mixer → 11.059 MHz IF → LARCset IF strip
                              ↑
                   Si5351 CLK1 at 132.941 MHz (currently disabled)
```

- LARCset unchanged — just sees 11 MHz in/out
- TX: LARCset IF out → ADE-1 → VHF BPF → VHF PA (2N3866 / MRF571)
- VHF PA is a separate sub-module
- Plugs into modular bus
- Si5351 CLK1 (currently disabled in firmware) provides transverter LO

**TODO:**
- [ ] Design VHF BPF (hairpin or small toroid, 144 MHz)
- [ ] Source ADE-1 or equivalent
- [ ] Design VHF PA stage
- [ ] Add CLK1 control to firmware

---

## 7. CAT Control `[idea]`
Computer control for digital modes (WSJT-X, JS8Call, logging).

- Simple Kenwood TS-480 protocol subset over Serial or WiFi TCP
- WSJT-X / JS8Call / Hamlib compatible
- WiFi CAT: already have AP infrastructure in firmware, add TCP server
- USB: original ESP32 has no native USB — use UART bridge or WiFi only

---

## 8. Digital Modes Interface `[idea]`
Get on FT8/JS8Call/WSPR.

- USB audio dongle (~$8) + laptop running WSJT-X or JS8Call
- CAT via WiFi (mod #7)
- Audio in/out wired to LARCset mic/speaker jacks
- WSPR beacon: fully self-contained on ESP32 via Si5351 (well-proven)

---

## 9. Improved PA LPF `[planned]`
Replace or supplement stock LPF for better harmonic suppression.

- 7-element Chebyshev or elliptic LPF per band
- Mandatory for legal HF operation
- Part of LPF bank module (mod #3)
- Well-characterised component values available for each band

---

## 10. PIN Diode T/R Switch `[idea]`
Replace G5V-2 relay with solid-state PIN diode switching.

- Microsecond switching vs relay milliseconds
- Silent, no mechanical wear
- Required for full QSK CW operation
- Uses RF diodes on hand (verify type — BA479, MA4PBL, or 1N4007 at HF)
- Part of T/R switch module (mod #3)

---

## 11. 630m Band (472–479 kHz) `[idea]`
Direct mod — no transverter needed. Physics favors lower frequencies.

**VFO:**
- Low-side injection: CLK0 = 10.584–10.591 MHz (IF - RF)
- Si5351 handles this trivially
- Add 630m band mode to firmware alongside 40m and 2m modes
- Sideband: USB (630m convention, opposite of 40m) — flip BFO offset

**What needs changing:**
- **Trifilar transformers (T1/T2/T3):** Rewind with more turns on larger core — magnetizing inductance insufficient at 475 kHz with HF winding
- **BPF:** New bandpass for 472–479 kHz — chunky components (hundreds of µH + large caps) but electrically simple
- **LPF on PA output:** Rescale existing topology for 475 kHz cutoff
- **Everything else:** IRF510 PA, 1N4148 mixer diodes, layout — all fine at MF

**Antenna options (quarter wave = 158m, not happening):**
- Short vertical (10–20m) + base loading coil — low efficiency (~1–5%) but workable
- Transmitting loop (3–10m diameter) — high Q, narrow BW, needs retuning, efficient
- Existing 40m dipole + matching network + loading coil — lossy but usable for WSPR

**Primary use case:** WSPR beacon — propagation is spectacular, transatlantic paths at night common, band is quiet. Phone/CW ops rare but growing.

**Regulatory (US):**
- 5W EIRP maximum (secondary to PLC systems)
- General class license required
- Must notify Utilities Telecom Council before operating

---

## Notes
- IF frequency: 11.059 MHz (5x crystal ladder filter Y1–Y5)
- PA: IRF510N (TO-220), Class AB
- Original mixer: 1N4148 diode ring + trifilar transformers T1/T2/T3
- Audio amp: LM386
- T/R relay: Omron G5V-2 DPDT
