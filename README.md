# Polly
A **MIDI proxy** sketch for the **Midiboy** (Arduino) platform - enabling a **Korg Volca Sample 2** to be used as a polyphonic (and multitimbral) sample player (when combined with a MIDI keyboard / controller).

Supports up to **9 channels**, either **separate**, **layered** or **grouped**. In addition to **pitch**, control changes such as **sample selection**, **lo-cut**, **pan** and **reverb** can be mapped in a variety of ways.

```
The Volca Sample 2 must be configured to run in (midi) multi-channel mode.
When active channels exceeds 6-7 sometimes an audible click can be heard upon sample playback.
```

Note: If a parameter is configured to be off (e.g. velocity) then *no control change will be transmitted*.
Setting **release time** will trigger a control value of 127 *upon a note on event* and the configured *release time on note off*.


## Channels (1-9)
### Input
Key(s) | Action
------------ | -------------
B | Next page
A | *As indicated on the screen*
U/D/L/R | Move between parameters / Change parameter value

### Settings
Parameter | Description
------------ | -------------
MIDI| Listen channel (ALL, 1 - 16)
NSTEAL| Enable note stealing (OFF / ON)
LOWER| Lower range key mapping
UPPER| Upper range key mapping
OFFSET| Amount to offset notes played
REL| Release time triggered by a note off event (OFF, 1 - 127)
VEL| Velocity control (OFF, RANDOM, KEYPRESS)
VELMIN| Minimum velocity
VELMAX| Maximum velocity (velocity will be scaled within this range)
VELINV| Invert the velocity (OFF / ON)
CUT| Lo-cut control (OFF, RANDOM, KEYPRESS)
CUTMIN| Minimum lo-cut
CUTMAX| Maximum lo-cut (lo-cut will be scaled within this range)
GROUP| Channel is assigned to a group (1-4) *Note; A channel assigned to a group will have MIDI and Note stealing overriden by the group settings*

## Groups (1-4)
### Input
Key(s) | Action
------------ | -------------
B | Next page
U/D/L/R | Move between parameters / Change parameter value

### Settings
Parameter | Description
------------ | -------------
MIDI| Listen channel (ALL, 1 - 16)
NSTEAL| Enable note stealing (OFF / ON)
SMODE| Sample selection mode (OFF, RANDOM, KEYPRESS, CYCLE)
SMPL 1| Sample number (OFF, 1 - 200)
SMPL 2| Sample number (OFF, 1 - 200)
SMPL 3| Sample number (OFF, 1 - 200)
SMPL 4| Sample number (OFF, 1 - 200)
RVRB 1| Reverb setting (OFF, ENABLE, DISABLE, RANDOM)
RVRB 2| Reverb setting (OFF, ENABLE, DISABLE, RANDOM)
RVRB 3| Reverb setting (OFF, ENABLE, DISABLE, RANDOM)
RVRB 4| Reverb setting (OFF, ENABLE, DISABLE, RANDOM)
PAN| Ping-pong panning width (OFF, 1 - 4)

## MIDI Buffer Monitor
### Input
Key(s) | Action
------------ | -------------
B | Next page
A | Reset active notes

## Save / Load
### Input
Key(s) | Action
------------ | -------------
B | Next page
U/D | Move between save slots
L/R | Hold to save / Hold to load

