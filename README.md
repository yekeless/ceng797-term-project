# Implementation and Performance Analysis of the Least Cluster Change (LCC) Algorithm in MANETs

## Abstract
This repository contains the complete implementation, simulation scenarios, and performance analysis tools for the Least Cluster Change (LCC) algorithm in Mobile Ad Hoc Networks (MANETs). The study investigates the trade-off between routing optimality and cluster stability under high-mobility conditions. The protocol logic includes a hysteresis mechanism to mitigate the "ripple effect" and is validated against varying node densities, mobility speeds, and physical layer impairments (LogNormal Shadowing and Rician Fading).

The full academic report detailing the methodology, mathematical models, and discussion of results is included in this repository as `Project_Report.pdf`.

## System Requirements and Dependencies
The simulation framework relies on the following software versions. Compatibility with older versions is not guaranteed.

* **Operating System:** Ubuntu 22.04 LTS (Tested) / Windows 10/11
* **Discrete Event Simulator:** OMNeT++ v6.0.3 (or later)
* **Network Framework:** INET Framework v4.5.2
* **Compiler:** GCC v11.4.0 (Linux) / MinGW (Windows)
* **Data Analysis:** Python 3.10+
    * `pandas` v2.0.0
    * `numpy` v1.24.0
    * `scipy` v1.10.0 (Required for Confidence Interval calculation)

## Repository Structure
The project directory is organized as follows:

* `/src`: Contains the C++ source codes (`LCC.cc`, `LCC.h`) and message definitions (`LccMessage.msg`).
* `/simulations`: Contains the OMNeT++ configuration file (`omnetpp.ini`) and network description files (`.ned`).
* `/analysis`: Python scripts used for parsing `.csv` results and calculating statistical metrics.
* `Project_Report.pdf`: The comprehensive academic report of this study.

## Key Features
1.  **LCC Application Module:** A self-contained module inheriting from `inet::ApplicationBase` that implements the distributed clustering logic.
2.  **Hysteresis Implementation:** The state machine enforces the stability condition where a cluster member retains its affiliation unless the link to the Cluster Head times out.
3.  **Custom Instrumentation:** The module includes internal counters to track role changes, packet delivery ratios (PDR), and end-to-end delay, exporting these metrics directly to CSV format.
4.  **Physical Layer Integration:** The simulation is tuned to operate with `IEEE 802.11` radio parameters, specifically calibrated (2mW Tx Power) to create multi-hop topologies within a 600m x 600m area.

## Reproduction of Results
To reproduce the experimental data presented in the report, follow these steps:

1.  **Build the Project:**
    Ensure the project references the INET framework within the OMNeT++ workspace and build the executable.

2.  **Run Simulations:**
    The `omnetpp.ini` file contains predefined configurations for specific test cases.

    * **Scalability Test (60 Nodes):**
        ```bash
        ./LCC_Project -u Cmdenv -c Nodes_60 -r 0..9
        ```
    * **Mobility Test (High Speed):**
        ```bash
        ./LCC_Project -u Cmdenv -c Speed_Fast -r 0..9
        ```
    * **Physical Layer Test (Shadowing):**
        ```bash
        ./LCC_Project -u Cmdenv -c Phy_Realistic -r 0..9
        ```

3.  **Statistical Analysis:**
    After simulation execution, use the provided Python script to compute the mean values and 95% Confidence Intervals:
    ```bash
    python3 analysis/calculate_results.py --input results/
    ```

## License
This project is open-source and intended for educational and research purposes.

**Author:** [Senin Adın]
