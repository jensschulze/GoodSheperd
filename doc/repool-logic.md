# Repool — User Logic

Each of the four tracks (Stack mode) or the single track (Spread mode) runs the following state machine. The active state is reflected by the **STATE LED** colour.

```mermaid
stateDiagram-v2
    direction TB

    [*] --> EMPTY

    EMPTY      --> RECORDING : REC press (Instant, or Synced with no loop yet)
    EMPTY      --> PENDING   : REC press (Synced — loop already exists)

    PENDING    --> RECORDING : loop start reached, button released or held under 100 ms — records one full loop
    PENDING    --> RECORDING : loop start reached, button held over 100 ms — hold-to-record
    PENDING    --> EMPTY     : CLR

    RECORDING  --> PLAYING   : REC release — snaps to nearest clock beat if CLOCK is patched
    RECORDING  --> EMPTY     : CLR or recording too short

    PLAYING    --> OVERDUBBING : OVD press (Instant) or loop start while OVD held (Synced)
    PLAYING    --> PLAYING     : RTG — reset playhead to 0, emit loop-start trigger
    PLAYING    --> PLAYING     : UND — remove last overdub layer
    PLAYING    --> EMPTY       : CLR

    OVERDUBBING --> PLAYING  : OVD release — commit layer
    OVERDUBBING --> PLAYING  : RTG — commit layer, reset playhead to 0, emit loop-start trigger
    OVERDUBBING --> PLAYING  : UND — discard in-progress layer
    OVERDUBBING --> EMPTY    : CLR

    note right of EMPTY
        LED colours
        EMPTY       off
        PENDING     yellow
        RECORDING   red
        PLAYING     green
        OVERDUBBING blue
    end note

    note right of PENDING
        SELECT is blocked during
        RECORDING, OVERDUBBING,
        and PENDING.
    end note

    note right of OVERDUBBING
        Stack mode  4 independent tracks
        RTG resets ALL tracks simultaneously.
        Linked option locks all loop lengths
        to the first recorded loop.

        Spread mode  1 track, max 3 layers
        OVD adds a new layer (columns = versions).
        SELECT rewinds to an earlier layer.
        REC is only available from EMPTY.
    end note
```

## Controls at a glance

| Button         | Action                                                                                                             |
| -------------- | ------------------------------------------------------------------------------------------------------------------ |
| **SELECT 1–4** | Choose active track (Stack) or rewind to layer version (Spread). Blocked while recording, overdubbing, or pending. |
| **REC**        | Hold to record. Release commits the loop (clock-quantized if patched).                                             |
| **OVD**        | Hold to overdub. Release commits the layer.                                                                        |
| **UND**        | Remove last committed layer, or discard an in-progress overdub.                                                    |
| **CLR**        | Clear the selected track back to EMPTY.                                                                            |
| **RTG**        | Reset all playing/overdubbing tracks to position 0. Fires a loop-start trigger on the LOOP START output.           |

## Outputs

| Output         | Signal                                                                           |
| -------------- | -------------------------------------------------------------------------------- |
| **AUDIO**      | Wet/dry mix controlled by the MIX knob.                                          |
| **POS**        | 0–10 V normalised playhead position of the master (first content-bearing) track. |
| **LOOP START** | 1 ms trigger on every loop wrap, and on every RTG press.                         |
