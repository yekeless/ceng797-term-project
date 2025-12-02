// LCC.ned
package src;

import inet.applications.contract.IApp;

simple LCC like IApp
{
    parameters:
        @class(inet::LCC);

        string interfaceTableModule = default("");

        int localPort = default(5000);
        int destPort = default(5000);

        double beaconInterval @unit(s) = default(1s);
        double neighborValidityInterval @unit(s) = default(3.5s);

        @display("i=block/network2");

    gates:
        input socketIn @labels(UdpControlInfo/up);
        output socketOut @labels(UdpControlInfo/down);
}
