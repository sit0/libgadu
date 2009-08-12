/* $Id$ */

/*
 *  (C) Copyright 2001-2006 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Woźny <speedy@ziew.org>
 *                          Arkadiusz Miśkiewicz <arekm@pld-linux.org>
 *                          Tomasz Chiliński <chilek@chilan.com>
 *                          Adam Wysocki <gophi@ekg.chmurka.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

/**
 * \file debug.c
 *
 * \brief Funkcje odpluskwiania
 */
#include <sys/types.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "libgadu.h"
#include "debug.h"

/**
 * Poziom rejestracji informacji odpluskwiających. Zmienna jest maską bitową
 * składającą się ze stałych \c GG_DEBUG_...
 *
 * \ingroup debug
 */
int gg_debug_level = 0;

/**
 * Funkcja, do której są przekazywane informacje odpluskwiające. Jeśli zarówno
 * ten \c gg_debug_handler, jak i \c gg_debug_handler_session, są równe
 * \c NULL, informacje są wysyłane do standardowego wyjścia błędu (\c stderr).
 *
 * \param level Poziom rejestracji
 * \param format Format wiadomości (zgodny z \c printf)
 * \param ap Lista argumentów (zgodna z \c printf)
 *
 * \note Funkcja jest przesłaniana przez \c gg_debug_handler_session.
 *
 * \ingroup debug
 */
void (*gg_debug_handler)(int level, const char *format, va_list ap) = NULL;

/**
 * Funkcja, do której są przekazywane informacje odpluskwiające. Jeśli zarówno
 * ten \c gg_debug_handler, jak i \c gg_debug_handler_session, są równe
 * \c NULL, informacje są wysyłane do standardowego wyjścia błędu.
 *
 * \param sess Sesja której dotyczy informacja lub \c NULL
 * \param level Poziom rejestracji
 * \param format Format wiadomości (zgodny z \c printf)
 * \param ap Lista argumentów (zgodna z \c printf)
 *
 * \note Funkcja przesłania przez \c gg_debug_handler_session.
 *
 * \ingroup debug
 */
void (*gg_debug_handler_session)(struct gg_session *sess, int level, const char *format, va_list ap) = NULL;

/**
 * Plik, do którego będą przekazywane informacje odpluskwiania.
 *
 * Funkcja \c gg_debug() i pochodne mogą być przechwytywane przez aplikację
 * korzystającą z biblioteki, by wyświetlić je na żądanie użytkownika lub
 * zapisać do późniejszej analizy. Jeśli nie określono pliku, wybrane
 * informacje będą wysyłane do standardowego wyjścia błędu (\c stderr).
 *
 * \ingroup debug
 */
FILE *gg_debug_file = NULL;

#ifndef GG_DEBUG_DISABLE

/**
 * \internal Przekazuje informacje odpluskwiania do odpowiedniej funkcji.
 *
 * Jeśli aplikacja ustawiła odpowiednią funkcję obsługi w
 * \c gg_debug_handler_session lub \c gg_debug_handler, jest ona wywoływana.
 * W przeciwnym wypadku wynik jest wysyłany do standardowego wyjścia błędu.
 *
 * \param sess Struktura sesji (może być \c NULL)
 * \param level Poziom informacji
 * \param format Format wiadomości (zgodny z \c printf)
 * \param ap Lista argumentów (zgodna z \c printf)
 */
void gg_debug_common(struct gg_session *sess, int level, const char *format, va_list ap)
{
	if (gg_debug_handler_session)
		(*gg_debug_handler_session)(sess, level, format, ap);
	else if (gg_debug_handler)
		(*gg_debug_handler)(level, format, ap);
	else if (gg_debug_level & level)
		vfprintf((gg_debug_file) ? gg_debug_file : stderr, format, ap);
}


/**
 * Przekazuje informację odpluskawiania.
 *
 * \param level Poziom wiadomości
 * \param format Format wiadomości (zgodny z \c printf)
 *
 * \ingroup debug
 */
void gg_debug(int level, const char *format, ...)
{
	va_list ap;
	int old_errno = errno;
	va_start(ap, format);
	gg_debug_common(NULL, level, format, ap);
	va_end(ap);
	errno = old_errno;
}

/**
 * Przekazuje informację odpluskwiania związaną z sesją.
 *
 * \param sess Struktura sesji
 * \param level Poziom wiadomości
 * \param format Format wiadomości (zgodny z \c printf)
 *
 * \ingroup debug
 */
void gg_debug_session(struct gg_session *gs, int level, const char *format, ...)
{
	va_list ap;
	int old_errno = errno;
	va_start(ap, format);
	gg_debug_common(gs, level, format, ap);
	va_end(ap);
	errno = old_errno;
}

/**
 * Przekazuje zrzut bufora do odpluskwiania.
 *
 * \param sess Struktura sesji
 * \param level Poziom wiadomości
 * \param buf Bufor danych
 * \param len Długość bufora danych
 *
 * \ingroup debug
 */
void gg_debug_dump(struct gg_session *gs, int level, const char *buf, int len)
{
	char line[80];
	int i, j;

	for (i = 0; i < len; i += 16) {
		int ofs;

		sprintf(line, "%.4x: ", i);
		ofs = 6;

		for (j = 0; j < 16; j++) {
			if (i + j < len)
				sprintf(line + ofs, " %02x", (unsigned char) buf[i + j]);
			else
				sprintf(line + ofs, "   ");

			ofs += 3;
		}

		sprintf(line + ofs, "  ");
		ofs += 2;

		for (j = 0; j < 16; j++) {
			unsigned char ch;

			if (i + j < len) {
				ch = buf[i + j];

				if (ch < 32 || ch > 126)
					ch = '.';
			} else {
				ch = ' ';
			}

			line[ofs++] = ch;
		}

		line[ofs++] = '\n';
		line[ofs++] = 0;

		gg_debug_session(gs, level, "%s", line);
	}
}

/**
 * Zwraca ciąg z nazwą podanego stanu sesji.
 *
 * \param state Stan sesji.
 *
 * \return Ciąg z nazwą stanu
 *
 * \ingroup debug
 */
const char *gg_debug_state(enum gg_state_t state)
{
	switch (state) {
#define GG_DEBUG_STATE(x) case x: return #x;
	GG_DEBUG_STATE(GG_STATE_IDLE)
	GG_DEBUG_STATE(GG_STATE_RESOLVING)
	GG_DEBUG_STATE(GG_STATE_CONNECTING)
	GG_DEBUG_STATE(GG_STATE_READING_DATA)
	GG_DEBUG_STATE(GG_STATE_ERROR)
	GG_DEBUG_STATE(GG_STATE_CONNECTING_HUB)
	GG_DEBUG_STATE(GG_STATE_CONNECTING_GG)
	GG_DEBUG_STATE(GG_STATE_READING_KEY)
	GG_DEBUG_STATE(GG_STATE_READING_REPLY)
	GG_DEBUG_STATE(GG_STATE_CONNECTED)
	GG_DEBUG_STATE(GG_STATE_SENDING_QUERY)
	GG_DEBUG_STATE(GG_STATE_READING_HEADER)
	GG_DEBUG_STATE(GG_STATE_PARSING)
	GG_DEBUG_STATE(GG_STATE_DONE)
	GG_DEBUG_STATE(GG_STATE_LISTENING)
	GG_DEBUG_STATE(GG_STATE_READING_UIN_1)
	GG_DEBUG_STATE(GG_STATE_READING_UIN_2)
	GG_DEBUG_STATE(GG_STATE_SENDING_ACK)
	GG_DEBUG_STATE(GG_STATE_READING_ACK)
	GG_DEBUG_STATE(GG_STATE_READING_REQUEST)
	GG_DEBUG_STATE(GG_STATE_SENDING_REQUEST)
	GG_DEBUG_STATE(GG_STATE_SENDING_FILE_INFO)
	GG_DEBUG_STATE(GG_STATE_READING_PRE_FILE_INFO)
	GG_DEBUG_STATE(GG_STATE_READING_FILE_INFO)
	GG_DEBUG_STATE(GG_STATE_SENDING_FILE_ACK)
	GG_DEBUG_STATE(GG_STATE_READING_FILE_ACK)
	GG_DEBUG_STATE(GG_STATE_SENDING_FILE_HEADER)
	GG_DEBUG_STATE(GG_STATE_READING_FILE_HEADER)
	GG_DEBUG_STATE(GG_STATE_GETTING_FILE)
	GG_DEBUG_STATE(GG_STATE_SENDING_FILE)
	GG_DEBUG_STATE(GG_STATE_READING_VOICE_ACK)
	GG_DEBUG_STATE(GG_STATE_READING_VOICE_HEADER)
	GG_DEBUG_STATE(GG_STATE_READING_VOICE_SIZE)
	GG_DEBUG_STATE(GG_STATE_READING_VOICE_DATA)
	GG_DEBUG_STATE(GG_STATE_SENDING_VOICE_ACK)
	GG_DEBUG_STATE(GG_STATE_SENDING_VOICE_REQUEST)
	GG_DEBUG_STATE(GG_STATE_READING_TYPE)
	GG_DEBUG_STATE(GG_STATE_TLS_NEGOTIATION)
	GG_DEBUG_STATE(GG_STATE_REQUESTING_ID)
	GG_DEBUG_STATE(GG_STATE_WAITING_FOR_ACCEPT)
	GG_DEBUG_STATE(GG_STATE_WAITING_FOR_INFO)
	GG_DEBUG_STATE(GG_STATE_READING_ID)
	GG_DEBUG_STATE(GG_STATE_SENDING_ID)
	GG_DEBUG_STATE(GG_STATE_DISCONNECTING)
	GG_DEBUG_STATE(GG_STATE_CONNECT_HUB)
	GG_DEBUG_STATE(GG_STATE_CONNECT_PROXY_HUB)
	GG_DEBUG_STATE(GG_STATE_CONNECT_GG)
	GG_DEBUG_STATE(GG_STATE_CONNECT_PROXY_GG)
	GG_DEBUG_STATE(GG_STATE_RESOLVE_HUB_SYNC)
	GG_DEBUG_STATE(GG_STATE_RESOLVE_HUB_ASYNC)
	GG_DEBUG_STATE(GG_STATE_RESOLVE_PROXY_SYNC)
	GG_DEBUG_STATE(GG_STATE_RESOLVE_PROXY_ASYNC)
	GG_DEBUG_STATE(GG_STATE_RESOLVING_HUB)
	GG_DEBUG_STATE(GG_STATE_RESOLVING_PROXY)

	// Celowo nie ma default, żeby kompilator wyłapał brakujące stany
	
	}

	return NULL;
}

#else

#undef gg_debug_common
void gg_debug_common(struct gg_session *sess, int level, const char *format, va_list ap)
{
}

#undef gg_debug
void gg_debug(int level, const char *format, ...)
{
}

#undef gg_debug_session
void gg_debug_session(struct gg_session *gs, int level, const char *format, ...)
{
}

#undef gg_debug_dump
void gg_debug_dump(struct gg_session *gs, int level, const char *buf, int len)
{
}

#endif
