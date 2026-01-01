package simulations;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.AdhocHost;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadioMedium;
import inet.visualizer.common.IntegratedVisualizer;

network Sim
{
    parameters:
        int numHosts = default(40); // Varsayılanı 40
        @display("bgb=600,600");    // Görsel alanı 600x600
    submodules:
        visualizer: IntegratedVisualizer {
            @display("p=100,50");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=100,100");
        }
        radioMedium: <default("Ieee80211ScalarRadioMedium")> like IRadioMedium {
            @display("p=100,200");
        }
        host[numHosts]: AdhocHost {
            @display("p=300,300");
        }
}
