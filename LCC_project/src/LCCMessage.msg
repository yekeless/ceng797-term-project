// LCCMessage.msg
import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;

namespace inet;

enum LccRole {
    UNDECIDED = 0;
    CLUSTER_MEMBER = 1;
    CLUSTER_HEAD = 2;
};

// 1. Yönetim Paketi (Beacon)
class LccBeacon extends FieldsChunk {
    int srcId;
    int role @enum(LccRole);
    int clusterHeadId;
}

// 2. Veri Paketi (YENİ - İstatistikler için)
class LccData extends FieldsChunk {
    int srcId;
    int destId;
    simtime_t sendTime; // Gecikme ölçmek için
    int sequenceNumber;
}
