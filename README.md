# Model Car remote controller

This project reads input signals from a standard model car receiver and forwards them to servo and motor control. To enable the usage of a grown-up model car by children, the ratio between input and output can be adapted as well as offsets and limits.

![My model car](docs/modelcar_sm.jpg)

Needed Hardware:
* EPS32S2 NodeMCU (e.g. EPS32-S2 Saola)
* a model car controlled by two PWM signals
* a WiFi device to access to configuration webserver

Configuration:
* use idy.py menuconfig to set parameters (e.g. WiFi Password, ...)

Software:
* ESP-IDF Package
* Visual Studio Code (with dev container support)

