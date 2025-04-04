#include "headers/process_info.h"


// Change enum to string
string get_state_char(ProcessState state) {
    switch (state) {
        case RUNNING: return "RUNNING";
        case SLEEPING: return "SLEEPING";
        case TERMINATED: return "TERMINATED";
        case IDLE: return "IDLE";
        default: return "UNKNOWN";  // Handle unknown state
    }
}