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

// LCCMessage.msg
import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;

namespace inet;

// Node'un o anki rolü (Durumu)
enum LccRole {
    UNDECIDED = 0;      // Henüz karar vermedi
    CLUSTER_MEMBER = 1; // Bir lidere bağlı üye
    CLUSTER_HEAD = 2;   // Lider (Cluster Head)
};

// Paket İçeriği (Node'lar bunu birbirine gönderecek)
class LccBeacon extends FieldsChunk {
    int srcId;               // Gönderen Node ID
    int role @enum(LccRole); // Gönderenin Rolü
    int clusterHeadId;       // Liderinin ID'si
}