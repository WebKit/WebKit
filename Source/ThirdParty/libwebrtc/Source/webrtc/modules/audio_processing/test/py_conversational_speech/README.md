#Conversational Speech generator tool

Python tool to generate multiple-end audio tracks to simulate conversational
speech with two or more participants.

The input to the tool is a directory containing a number of audio tracks and
a text file indicating how to time the sequence of speech turns (see the Example
section).

Since the timing of the speaking turns is specified by the user, the generated
tracks may not be suitable for testing scenarios in which there is unpredictable
network delay (e.g., end-to-end RTC assessment).

Instead, the generated pairs can be used when the delay is constant (obviously
including the case in which there is no delay).
For instance, echo cancellation in the APM module can be evaluated using two-end
audio tracks as input and reverse input.

By indicating negative and positive time offsets, one can reproduce cross-talk
and silence in the conversation.

IMPORTANT: **the whole code has not been landed yet.**

###Example

For each end, there is a set of audio tracks, e.g., a1, a2 and a3 (speaker A)
and b1, b2 (speaker B).
The text file with the timing information may look like this:
```  A a1 0
  B b1 0
  A a2 100
  B b2 -200
  A a3 0
  A a4 0```
The first column indicates the speaker name, the second contains the audio track
file names, and the third the offsets (in milliseconds) used to concatenate the
chunks.

Assume that all the audio tracks in the example above are 1000 ms long.
The tool will then generate two tracks (A and B) that look like this:

```Track A:
  a1 (1000 ms)
  silence (1100 ms)
  a2 (1000 ms)
  silence (800 ms)
  a3 (1000 ms)
  a4 (1000 ms)```

```Track B:
  silence (1000 ms)
  b1 (1000 ms)
  silence (900 ms)
  b2 (1000 ms)
  silence (2000 ms)```

The two tracks can be also visualized as follows (one characheter represents
100 ms, "." is silence and "*" is speech).

```t: 0         1        2        3        4        5        6 (s)
A: **********...........**********........********************
B: ..........**********.........**********....................
                                ^ 200 ms cross-talk
        100 ms silence ^```
