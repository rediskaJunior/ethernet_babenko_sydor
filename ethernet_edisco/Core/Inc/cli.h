/**
 * @file cli.h
 * @brief Command Line Interface Header
 */

#ifndef CLI_H
#define CLI_H

#include "main.h"

/**
 * @brief Initialize CLI system
 */
void cli_init(void);

/**
 * @brief Process CLI input (call in main loop)
 */
void cli_process(void);

#endif /* CLI_H */
