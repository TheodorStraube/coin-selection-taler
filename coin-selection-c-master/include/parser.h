//
// Created by Bohdan Potuzhnyi and Vlada Svirsh on 10.03.2024.
// parser.h
//

#ifndef PARSER_H
#define PARSER_H

#include "common.h"

Wallet parse_wallet_config(const char* config_path);
Wallet parse_wallet_config_json(const char* config_path);

long long time_to_seconds(const char* time_str);

#endif //PARSER_H
