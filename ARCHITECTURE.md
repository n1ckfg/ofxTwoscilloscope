# ofxTwoscilloscope Architecture

## Overview

ofxTwoscilloscope joins two oscilloscope projects at the audio between them:

* the encoding half of **XYscope** (Processing, Java), which turns vector drawings into
  XY audio, and
* the decoding half of **Oscilloscope** (openFrameworks, C++), which turns XY audio
  into an image of an analog beam,

and adds what neither had: decoding the audio back into vector shapes, and using
encode, process, decode as a way to transform one vector shape into another.

```
                    XYscope                      XYEffectChain                XYDecoder
 shapes (canvas px) -------> XY(Z) audio loop ------------------> altered audio ---------> shapes (canvas px)
      ^                          |                                     |
      |                          v                                     v
  HersheyFont              sound card / WavFile                  Oscilloscope + OsciMesh
                                                                 (beam image in an FBO)
```

Everything is in `src/`. `ofxTwoscilloscope.h` includes it all.

## Encoding (from XYscope)

### `XYscope`
A port of `XYscope.java`.

* **Shape buffering**: drawing calls (`rect()`, `ellipse()`, `vertex()`, `text()`...)
  append points to `shapes`, normalized to 0..1 on the canvas. The last point of each
  shape is flagged (z = 1) so the beam can blank there.
* **Transforms**: Processing's matrix stack and `screenX()`/`screenY()` are replaced by
  XYscope's own `glm::mat4` stack. 3D points are projected the way Processing's default
  P3D camera does (60° field of view, eye far enough back that z = 0 maps 1:1 to
  pixels), so 2D drawing is unaffected.
* **buildWaves()**: the "v4" algorithm. Every segment is interpolated with a number of
  steps in proportion to its share of the total path length (`steps()` x points), so the
  beam moves at an even speed. The result is resampled to `waveSize()` entries per axis
  and mapped to -1..1 (Y up). Z is `zMax` except at the end of a shape, where it is
  `zMin`. The tables go to `tableX`, `tableY` and `tableZ`.
* **Oscillators**: in place of Minim's `Oscil` and `Pan` ugens, `synth()` reads the
  tables with a phase accumulator at `freq()` Hz, scales by `amp()`, and pans X and Y
  with Minim's equal-power law. It writes X, Y and Z to channels 0, 1 and 2.
* **Outputs**: `audioOut()` (an `ofBaseSoundOutput`, either for an `ofSoundStream` that
  XYscope opens itself or for one the app owns), `audioOutAdd()` for mixing, `process()`
  to run without a sound card, and `render()` for offline audio with its own oscillator
  state. Live output feeds a rolling preview buffer (for `drawXY()` and `drawWave()`)
  and the recorder.

### `XYWavetable`
A port of `XYWavetable.java`. The table is a `shared_ptr<const vector<float>>` swapped
under a mutex. The audio thread takes the current table once per buffer and keeps
reading it even if the drawing thread swaps in a new one, which is the
array-out-of-bounds problem the Java class was written to fix. Lookups interpolate
linearly and wrap.

### `HersheyFont`
XYscope's Hershey text engine as a class. It parses `.jhf` files by vertex count (so
wrapped glyphs parse correctly) and lays out multi-line, aligned text as `ofPolyline`
strokes. `futural` is compiled in (`HersheyFutural.h`).

## Decoding (from Oscilloscope)

### `Oscilloscope`
The update/draw pipeline of the Oscilloscope app's `ofApp`, as a component.

* **Input** (any thread): `addSamples()` / `addBuffer()` / `audioIn()`. Under one mutex,
  each channel goes through a `StreamResampler` to the visual sample rate (192kHz by
  default) into a pending queue, and the source-rate samples go into a rolling history
  for decoding. The channel count picks the layout: MONO, STEREO, STEREO_ZMODULATED or
  QUAD, as the app's `OsciAvAudioPlayer::FileType` did.
* **update()** (main thread): swaps out the pending samples and builds the beam meshes.
  The queue is capped (1/15 s), so a slow frame drops samples instead of building an
  ever bigger mesh, as the app's "dropped" counter did.
* **draw()**: fades the FBO by `afterglow` (a black rectangle with multiply blending),
  draws the meshes additively in scope units (-1..1, Y up, fit to the shorter side), and
  draws the FBO without blending.
* **getShapes()**: copies the last half second of history and runs `XYDecoder` on it.
  It keeps the previous loop period while `XYDecoder::periodError()` says the signal
  still repeats at it, so detection only runs when the loop changes.

### `OsciMesh`
A port of the app's `OsciMesh`. Each pair of samples becomes a quad around their segment
(6 vertices). Positions carry the brightness in z, normals carry the segment-space
coordinates and length. The fragment shader integrates a gaussian beam along the segment
with an erf approximation (after m1el's woscope), so light spreads thinly over fast
segments and piles up on slow ones. The shader is generated for the running renderer:
GLSL 1.20 for GL 2, `#version` 140+ with `in`/`out` for GL 3+, and GLSL ES 1.00 for GLES.

### `StreamResampler`
Streaming sample rate conversion in place of FFmpeg's swresample: Lanczos (4 lobes,
normalized weights) or linear. It keeps just enough history between calls to stay
continuous.

### `XYPlayer`
Plays a WAV to the sound card and feeds the same file-rate samples to an `Oscilloscope`,
which is the job `OsciAvAudioPlayer` did. It resamples to the device rate and channel
count, and `update(seconds)` plays on the clock when there's no device.

### `XYDecoder`
New. It turns XY audio back into shapes on a canvas:

1. **Period**: `sampleRate / freq` when the loop frequency is known. Otherwise YIN (the
   cumulative mean normalized difference of X and Y together) on a decimated copy, then
   a refinement at full rate with parabolic interpolation for a fractional period.
2. **Mapping**: the most recent full loop, mapped back with the inverse of XYscope's
   mapping.
3. **Strokes**: cut where Z falls below `zThreshold` (keeping the sample where the beam
   blanks, which in XYscope is the shape's last point) or where a step is longer than
   `jumpFactor` x the median step.
4. **Loop seam**: the stroke that runs off the end of the loop joins the one at the
   start. Strokes whose ends are a couple of steps apart are closed.
5. **Cleanup**: duplicate points removed, Douglas-Peucker `simplify`, short strokes
   dropped.

`saveSvg()` writes the result as SVG paths.

## Transforming (new)

### `XYEffect` and `XYEffectChain`
Effects work in place on channels 0 and 1 of an interleaved `ofSoundBuffer`, sample by
sample (`prepare()` once per buffer, then `processFrame(x, y)`). Z passes through
untouched, so time-based effects move the beam relative to its blanking, just as they
would if only X and Y went through a real effects unit. Settings are `ofParameter`s in
one `ofParameterGroup` per effect, collected by the chain for ofxGui. Every effect can
`reset()`, and the noise is seeded, so a transform is repeatable.

### `XYTransformer`
1. Shapes go into a private `XYscope` (`polylines()`, `buildWaves()`).
2. `XYscope::render()` makes `settleCycles + 1` loops of 3-channel audio.
3. The chain is reset and run over the whole buffer.
4. The last loop (`sampleRate / freq` samples) goes to `XYDecoder::decodeCycle()`, with
   the period known, so nothing is guessed.

The processed loop is exposed as audio (`getProcessedCycle()`), so an `XYscope` can
loop it with `setWaveforms()`: the altered shape becomes playable XYscope audio again.

## Examples

* **`example-encode`**: vector shapes -> XYscope format audio. Draws a scene into an `XYscope` to output X (left) and Y (right) audio.
* **`example-decode`**: XYscope format audio -> vector shapes. Plays an audio file (or line input) through an `Oscilloscope` to render an analog beam, and decodes the audio back into vector shapes.
* **`example-transform`**: vector shape -> XY audio -> audio effects -> new vector shape. Encodes a shape into audio, runs it through an `XYEffectChain`, and decodes it back, playing the result out the sound card.
* **`example-latk`**: projects a 3D Latk animation (typically drawn in VR) to 2D, encodes it, transforms it with an `XYEffectChain`, and draws the altered audio with an `OsciMesh`. Uses a custom `LatkScopeRenderer` to keep track of individual strokes through the pipeline so they can be drawn in their original colors.

## Files

### `WavFile`
A dependency-free RIFF/WAVE reader (8/16/24/32-bit PCM, 32/64-bit float, extensible
headers) and writer (16/24-bit PCM, 32-bit float), so neither half needs FFmpeg or
libsndfile. XYscope's recorder writes WAV, and 3-channel WAVs carry Z.

## Threading

| Object | Audio thread | Main thread | Shared through |
| --- | --- | --- | --- |
| `XYscope` | `audioOut()` | drawing, `buildWaves()`, settings | `XYWavetable` swaps, a parameter mutex, an oscillator mutex, a preview/recorder mutex |
| `Oscilloscope` | `addSamples()` | `update()`, `draw()`, `getShapes()` | one mutex around the resamplers, queue and history |
| `XYPlayer` | `audioOut()` | transport controls | one mutex |
| `XYTransformer` | none | everything | nothing shared |

`XYEffect`s keep state and aren't locked. Use a chain on one thread only, or give each
thread its own chain.
