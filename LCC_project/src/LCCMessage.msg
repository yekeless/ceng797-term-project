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

// Paket İçeriği
class LccBeacon extends FieldsChunk {
    int srcId;               // Gönderen Node ID
    int role @enum(LccRole); // Gönderenin Rolü
    int clusterHeadId;       // Liderinin ID'si
}
