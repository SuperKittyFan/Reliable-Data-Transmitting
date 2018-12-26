# Reliable-Data-Transmitting
This is UIUC CS438 communication network course project. The objective is to implement a TCP-like reliable data transmitting protocol based on UDP network programming in C++.

* Two instances of this protocol competing with each other converge to roughly fairly sharing the link (same throughputs ±10%), within 100 RTTs. The two instances might not be started at the exact same time.
* This protocol is TCP friendly: an instance of TCP competing must get on average at least half as much throughput as the flow.
* An instance of this protocol competing with TCP get on average at least half as much throughput as the TCP flow. 
* All of the above should hold in the presence of any amount of dropped packets. All flows, including the TCP flows, will see the same rate of drops. The network will not introduce bit errors.
* The protocol utilize at 80% of band-width when there is no competing traffic, and packets are not artificially dropped or re-ordered.
