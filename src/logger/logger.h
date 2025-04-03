#ifndef LOGGER_H
#define LOGGER_H

#include <any>
#include "../shared/process_info.h"
#include "../shared/util.h"

void createReportsRepository();
void saveReport(string app_name, Processinfo& details);

#endif