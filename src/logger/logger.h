#ifndef LOGGER_H
#define LOGGER_H

#include "../shared/headers/process_info.h"
#include "../shared/headers/util.h"
#include "../shared/headers/constants.h"

void createReportsRepository();
void saveReport(string app_name, Processinfo& details);

#endif