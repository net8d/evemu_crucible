/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2008 The EVEmu Team
    For the latest information visit http://evemu.mmoforge.org
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:     Zhur
*/


#ifndef LOGSYS_H_
#define LOGSYS_H_

/*
 *
 * Usage:
 *
 *  These are the main functions provided by logsys:
 *      - _log(TYPE, fmt, ...) - Log a message in any context
 *      - _hex(TYPE, data, length) - Log hex dump in any context.
 * Types are defined in logtypes.h
 *
 *
 *
 *
 * this is very C-ish, not C++ish, but thats how I felt like writting it
 *
 *
 *
 */

#include "common.h"

#ifdef WIN32
    //only VC2003+ has variadic macro support
    // 1200 = vc6
    // 1300 = vc2003
    // 1310 = vc8
    //NOTE: per-mob toggling of log messages cannot work without variadic macros!
    #if (_MSC_VER <= 1310)
        #define NO_VARIADIC_MACROS
    #endif

    #ifdef NO_VARIADIC_MACROS
        /*
        uncomment this to disable debug logging all together on windows
        this is here because logging has a decent amount of overhead
        on windows since we must make the call and build the variable
        argument list before checking to see if the type is enabled.
        */
        #define DISABLE_LOGSYS
    #endif
#endif


#define LOG_CATEGORY(category) LOG_ ##category ,
typedef enum {
    #include "logtypes.h"
    NUMBER_OF_LOG_CATEGORIES
} LogCategory;

#define LOG_TYPE(category, type, enabled, str) category##__##type ,
typedef enum {
    #include "logtypes.h"
    NUMBER_OF_LOG_TYPES
} LogType;

extern const char *log_category_names[NUMBER_OF_LOG_CATEGORIES];

typedef struct {
    bool enabled;
    LogCategory category;
    const char *name;
    const char *display_name;
} LogTypeStatus;

//expose a read-only pointer
extern const LogTypeStatus *log_type_info;

extern void log_message(LogType type, const char *fmt, ...);
extern void log_messageVA(LogType type, const char *fmt, va_list args);
extern void log_messageVA(LogType type, uint32 iden, const char *fmt, va_list args);
extern void log_hex(LogType type, const void *data, unsigned long length, unsigned char padding=4);
extern void log_phex(LogType type, const void *data, unsigned long length, unsigned char padding=4);

#ifdef DISABLE_LOGSYS
    //completely disabled, this is the best I can come up with since we have no variadic macros
    inline void _log(LogType, const char *, ...) {}
    inline void codelog(LogType, const char *, ...) {}
#else   //!DISABLE_LOGSYS
    #ifdef NO_VARIADIC_MACROS
        inline void _log(LogType type, const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            if(log_type_info[type].enabled) {
                log_messageVA(type, fmt, args);
            }
            va_end(args);
        }
        inline void codelog(LogType type, const char *fmt, ...) {
            //we cannot actually implement this one without variadic
            va_list args;
            va_start(args, fmt);
            if(log_type_info[type].enabled) {
                log_messageVA(type, fmt, args);
            }
            va_end(args);
        }
    #else //!NO_VARIADIC_MACROS
        //we have variadic macros, hooray!
        //the do-while construct is needed to allow a ; at the end of log(); lines when used
        //in conditional statements without {}'s
        #define _log( type, format, ...) \
                if(log_type_info[ type ].enabled) \
                    log_message(type, format, ##__VA_ARGS__)

        #define codelog( type, format, ...) \
                if(log_type_info[ type ].enabled) \
                    log_message(type, "%s(%s:%d): " format, \
                        __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
    #endif //!NO_VARIADIC_MACROS
#endif  //!DISABLE_LOGSYS

#define _hex( type, data, len) \
        if(log_type_info[ type ].enabled) \
            log_hex(type, (const char *)data, len)

#define phex( type, data, len) \
        if(log_type_info[ type ].enabled) \
            log_phex(type, (const char *)data, len)

extern void log_enable(LogType t);
extern void log_disable(LogType t);
extern void log_toggle(LogType t);

extern bool log_open_logfile(const char *file);
extern bool log_close_logfile(const char *fname);

bool load_log_settings(const char *filename);

#define is_log_enabled( type ) \
    log_type_info[ type ].enabled


#endif /*LOGSYS_H_*/






