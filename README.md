# GoodSheperd VCV Rack plugins

A collection of plugins I needed but could not find.

## Hurdle

![Hurdle](./doc/hurdle.png)

An uncertain switch: Depending on the voltage at the **P**robability Input, a signal at the gate input may or may not show up at the gate output.

* **P** in: Switching probability. 0-10V. The higher the input voltage, the more likely there will be a Gate signal at the **Gate out**put. 0V-10V corresponds to 0%-100%.
* **Gate in**: Gate Signal (0/10V). When a rising edge occurs, the voltage at the **P** input is being sampled as the probability.
* **Gate out**: Gate Signal (0/10V).
