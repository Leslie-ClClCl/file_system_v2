#ifndef Shell_h
#define Shell_h

#include <stdio.h>
#include "core_func.h"
#include "IO_operations.h"

bool shell(void);
void getCmdContent(std::string cmd, std::vector<std::string> &cmd_content);
#endif /* Shell_h */
