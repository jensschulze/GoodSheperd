# GoodSheperd VCV Rack plugins

A collection of plugins I use for live performances. And yes, it’s a typo but I won’t fix it.

## Stable16

![Stable16](./doc/stable16.png)

An eight track gate sequencer with independent start/end points and nudge functionality.

**Caveat:** it is very likely that this thing will grow a few more units in the foreseeable future. So if you use it in your patches please give it some space. ;)

## Hurdle

![Hurdle](./doc/hurdle.png)

An uncertain switch: Depending on the voltage at the **P**robability Input, a signal at the gate input may or may not trigger the gate output.

* **P** in: Switching probability. 0-10V. The higher the input voltage, the more likely there will be a Gate signal at the **Gate out**put. 0V-10V corresponds to 0%-100%.
* **Gate in**: Gate Signal (0/10V). When a rising edge occurs, the voltage at the **P** input is being sampled as the probability.
* **Gate out**: Gate Signal (0/10V). Based on the **P** input voltage a gate signal may or may not be present at this output.

## SEQ3st

![SEQ3st](./doc/seq3st.png)

A modified [VCV Rack Fundamental SEQ3](https://vcvrack.com/Fundamental.html) with stochastic gate outs per row. So you may use the CV as gate probability for the given step in the given row.

* **P Gate** out 1-3: Gate Signal (0/10V). Based on the current CV value a gate signal may or may not be present at this output.

## Stall

![Stall](./doc/stall.png)

Splits trigger/gate signals by a control voltage. Accepts control voltages corresponding to midi notes 35 to 82.

* **CV in**
* **Gate/Trigger in**
* **Gate/Trigger out 35-82**
