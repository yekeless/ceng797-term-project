// LCC.ned
package src;

import inet.applications.contract.IApp;

simple LCC like IApp
{
    parameters:
        @class(inet::LCC);

        // --- İSTATİSTİK TANIMLARI ---
        // Stability & Lifetime
        @signal[chChangeSignal](type="long");
        @statistic[chChangeCount](source="chChangeSignal"; record=count,vector; title="Number of CH Changes");

        @signal[chLifetimeSignal](type="simtime_t");
        @statistic[chLifetime](source="chLifetimeSignal"; record=mean,vector; title="CH Lifetime");

        // Overhead
        @signal[controlOverheadSignal](type="long");
        @statistic[controlOverhead](source="controlOverheadSignal"; record=sum,vector; title="Control Overhead");

        // Cluster Size
        @signal[clusterSizeSignal](type="long");
        @statistic[clusterSize](source="clusterSizeSignal"; record=mean,max,vector; title="Cluster Size");

        // Performance (PDR & Delay)
        @signal[rttSignal](type="simtime_t");
        @statistic[endToEndDelay](source="rttSignal"; record=mean,vector; title="End-to-End Delay");

        @signal[pdrSignal](type="double");
        @statistic[packetDeliveryRatio](source="pdrSignal"; record=mean; title="Packet Delivery Ratio");

        @signal[dataSentSignal](type="long");
        @statistic[dataSent](source="dataSentSignal"; record=count; title="Data Packets Sent");
        // ---------------------------
		
		bool useMulticast = default(true); // true: Phase 3 (Multicast), false: Phase 4 (Unicast/AODV)
        int numHosts = default(20);        // Rastgele hedef seçmek için toplam node sayısı
		
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
