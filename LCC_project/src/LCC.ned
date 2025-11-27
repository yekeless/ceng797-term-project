//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

// LCC.ned
package src;

import inet.applications.contract.IApp;

simple LCC like IApp
{
    parameters:
        // --- İŞTE BU SATIR EKSİK, BUNU EKLE: ---
        @class(inet::LCC);
        // ---------------------------------------

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
