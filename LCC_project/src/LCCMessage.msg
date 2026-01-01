// LCCMessage.msg
import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;

namespace inet;

enum LccRole {
    UNDECIDED = 0;
    CLUSTER_MEMBER = 1;
    CLUSTER_HEAD = 2;
};


class LccBeacon extends FieldsChunk {
    int srcId;
    int role;
    int clusterHeadId;
    int seenClusterIds[];
}

class LccData extends FieldsChunk {
    int srcId;
    int destId;
    simtime_t sendTime;
    int seqNo; 
}
