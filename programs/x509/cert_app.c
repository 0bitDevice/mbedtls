/*
 *  Certificate reading application
 *
 *  Copyright (C) 2006-2013, ARM Limited, All Rights Reserved
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#define mbedtls_fprintf    fprintf
#define mbedtls_printf     printf
#endif

#if !defined(MBEDTLS_BIGNUM_C) || !defined(MBEDTLS_ENTROPY_C) ||  \
    !defined(MBEDTLS_SSL_TLS_C) || !defined(MBEDTLS_SSL_CLI_C) || \
    !defined(MBEDTLS_NET_C) || !defined(MBEDTLS_RSA_C) ||         \
    !defined(MBEDTLS_X509_CRT_PARSE_C) || !defined(MBEDTLS_FS_IO) ||  \
    !defined(MBEDTLS_CTR_DRBG_C)
int main( void )
{
    mbedtls_printf("MBEDTLS_BIGNUM_C and/or MBEDTLS_ENTROPY_C and/or "
           "MBEDTLS_SSL_TLS_C and/or MBEDTLS_SSL_CLI_C and/or "
           "MBEDTLS_NET_C and/or MBEDTLS_RSA_C and/or "
           "MBEDTLS_X509_CRT_PARSE_C and/or MBEDTLS_FS_IO and/or "
           "MBEDTLS_CTR_DRBG_C not defined.\n");
    return( 0 );
}
#else

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODE_NONE               0
#define MODE_FILE               1
#define MODE_SSL                2

#define DFL_MODE                MODE_NONE
#define DFL_FILENAME            "cert.crt"
#define DFL_CA_FILE             ""
#define DFL_CRL_FILE            ""
#define DFL_CA_PATH             ""
#define DFL_SERVER_NAME         "localhost"
#define DFL_SERVER_PORT         4433
#define DFL_DEBUG_LEVEL         0
#define DFL_PERMISSIVE          0

#define USAGE_IO \
    "    ca_file=%%s          The single file containing the top-level CA(s) you fully trust\n" \
    "                        default: \"\" (none)\n" \
    "    crl_file=%%s         The single CRL file you want to use\n" \
    "                        default: \"\" (none)\n" \
    "    ca_path=%%s          The path containing the top-level CA(s) you fully trust\n" \
    "                        default: \"\" (none) (overrides ca_file)\n"

#define USAGE \
    "\n usage: cert_app param=<>...\n"                  \
    "\n acceptable parameters:\n"                       \
    "    mode=file|ssl       default: none\n"           \
    "    filename=%%s         default: cert.crt\n"      \
    USAGE_IO                                            \
    "    server_name=%%s      default: localhost\n"     \
    "    server_port=%%d      default: 4433\n"          \
    "    debug_level=%%d      default: 0 (disabled)\n"  \
    "    permissive=%%d       default: 0 (disabled)\n"  \
    "\n"

/*
 * global options
 */
struct options
{
    int mode;                   /* the mode to run the application in   */
    const char *filename;       /* filename of the certificate file     */
    const char *ca_file;        /* the file with the CA certificate(s)  */
    const char *crl_file;       /* the file with the CRL to use         */
    const char *ca_path;        /* the path with the CA certificate(s) reside */
    const char *server_name;    /* hostname of the server (client only) */
    int server_port;            /* port on which the ssl service runs   */
    int debug_level;            /* level of debugging                   */
    int permissive;             /* permissive parsing                   */
} opt;

static void my_debug( void *ctx, int level, const char *str )
{
    if( level < opt.debug_level )
    {
        mbedtls_fprintf( (FILE *) ctx, "%s", str );
        fflush(  (FILE *) ctx  );
    }
}

static int my_verify( void *data, mbedtls_x509_crt *crt, int depth, int *flags )
{
    char buf[1024];
    ((void) data);

    mbedtls_printf( "\nVerify requested for (Depth %d):\n", depth );
    mbedtls_x509_crt_info( buf, sizeof( buf ) - 1, "", crt );
    mbedtls_printf( "%s", buf );

    if( ( (*flags) & MBEDTLS_BADCERT_EXPIRED ) != 0 )
        mbedtls_printf( "  ! server certificate has expired\n" );

    if( ( (*flags) & MBEDTLS_X509_BADCERT_REVOKED ) != 0 )
        mbedtls_printf( "  ! server certificate has been revoked\n" );

    if( ( (*flags) & MBEDTLS_X509_BADCERT_CN_MISMATCH ) != 0 )
        mbedtls_printf( "  ! CN mismatch\n" );

    if( ( (*flags) & MBEDTLS_X509_BADCERT_NOT_TRUSTED ) != 0 )
        mbedtls_printf( "  ! self-signed or not signed by a trusted CA\n" );

    if( ( (*flags) & MBEDTLS_X509_BADCRL_NOT_TRUSTED ) != 0 )
        mbedtls_printf( "  ! CRL not trusted\n" );

    if( ( (*flags) & MBEDTLS_X509_BADCRL_EXPIRED ) != 0 )
        mbedtls_printf( "  ! CRL expired\n" );

    if( ( (*flags) & MBEDTLS_BADCERT_OTHER ) != 0 )
        mbedtls_printf( "  ! other (unknown) flag\n" );

    if ( ( *flags ) == 0 )
        mbedtls_printf( "  This certificate has no flags\n" );

    return( 0 );
}

int main( int argc, char *argv[] )
{
    int ret = 0, server_fd;
    unsigned char buf[1024];
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;
    mbedtls_x509_crl cacrl;
    mbedtls_pk_context pkey;
    int i, j;
    int flags, verify = 0;
    char *p, *q;
    const char *pers = "cert_app";

    /*
     * Set to sane values
     */
    server_fd = 0;
    mbedtls_x509_crt_init( &cacert );
    mbedtls_x509_crt_init( &clicert );
#if defined(MBEDTLS_X509_CRL_PARSE_C)
    mbedtls_x509_crl_init( &cacrl );
#else
    /* Zeroize structure as CRL parsing is not supported and we have to pass
       it to the verify function */
    memset( &cacrl, 0, sizeof(mbedtls_x509_crl) );
#endif
    mbedtls_pk_init( &pkey );

    if( argc == 0 )
    {
    usage:
        mbedtls_printf( USAGE );
        ret = 2;
        goto exit;
    }

    opt.mode                = DFL_MODE;
    opt.filename            = DFL_FILENAME;
    opt.ca_file             = DFL_CA_FILE;
    opt.crl_file            = DFL_CRL_FILE;
    opt.ca_path             = DFL_CA_PATH;
    opt.server_name         = DFL_SERVER_NAME;
    opt.server_port         = DFL_SERVER_PORT;
    opt.debug_level         = DFL_DEBUG_LEVEL;
    opt.permissive          = DFL_PERMISSIVE;

    for( i = 1; i < argc; i++ )
    {
        p = argv[i];
        if( ( q = strchr( p, '=' ) ) == NULL )
            goto usage;
        *q++ = '\0';

        for( j = 0; p + j < q; j++ )
        {
            if( argv[i][j] >= 'A' && argv[i][j] <= 'Z' )
                argv[i][j] |= 0x20;
        }

        if( strcmp( p, "mode" ) == 0 )
        {
            if( strcmp( q, "file" ) == 0 )
                opt.mode = MODE_FILE;
            else if( strcmp( q, "ssl" ) == 0 )
                opt.mode = MODE_SSL;
            else
                goto usage;
        }
        else if( strcmp( p, "filename" ) == 0 )
            opt.filename = q;
        else if( strcmp( p, "ca_file" ) == 0 )
            opt.ca_file = q;
        else if( strcmp( p, "crl_file" ) == 0 )
            opt.crl_file = q;
        else if( strcmp( p, "ca_path" ) == 0 )
            opt.ca_path = q;
        else if( strcmp( p, "server_name" ) == 0 )
            opt.server_name = q;
        else if( strcmp( p, "server_port" ) == 0 )
        {
            opt.server_port = atoi( q );
            if( opt.server_port < 1 || opt.server_port > 65535 )
                goto usage;
        }
        else if( strcmp( p, "debug_level" ) == 0 )
        {
            opt.debug_level = atoi( q );
            if( opt.debug_level < 0 || opt.debug_level > 65535 )
                goto usage;
        }
        else if( strcmp( p, "permissive" ) == 0 )
        {
            opt.permissive = atoi( q );
            if( opt.permissive < 0 || opt.permissive > 1 )
                goto usage;
        }
        else
            goto usage;
    }

    /*
     * 1.1. Load the trusted CA
     */
    mbedtls_printf( "  . Loading the CA root certificate ..." );
    fflush( stdout );

    if( strlen( opt.ca_path ) )
    {
        ret = mbedtls_x509_crt_parse_path( &cacert, opt.ca_path );
        verify = 1;
    }
    else if( strlen( opt.ca_file ) )
    {
        ret = mbedtls_x509_crt_parse_file( &cacert, opt.ca_file );
        verify = 1;
    }

    if( ret < 0 )
    {
        mbedtls_printf( " failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n", -ret );
        goto exit;
    }

    mbedtls_printf( " ok (%d skipped)\n", ret );

#if defined(MBEDTLS_X509_CRL_PARSE_C)
    if( strlen( opt.crl_file ) )
    {
        if( ( ret = mbedtls_x509_crl_parse_file( &cacrl, opt.crl_file ) ) != 0 )
        {
            mbedtls_printf( " failed\n  !  mbedtls_x509_crl_parse returned -0x%x\n\n", -ret );
            goto exit;
        }

        verify = 1;
    }
#endif

    if( opt.mode == MODE_FILE )
    {
        mbedtls_x509_crt crt;
        mbedtls_x509_crt *cur = &crt;
        mbedtls_x509_crt_init( &crt );

        /*
         * 1.1. Load the certificate(s)
         */
        mbedtls_printf( "\n  . Loading the certificate(s) ..." );
        fflush( stdout );

        ret = mbedtls_x509_crt_parse_file( &crt, opt.filename );

        if( ret < 0 )
        {
            mbedtls_printf( " failed\n  !  mbedtls_x509_crt_parse_file returned %d\n\n", ret );
            mbedtls_x509_crt_free( &crt );
            goto exit;
        }

        if( opt.permissive == 0 && ret > 0 )
        {
            mbedtls_printf( " failed\n  !  mbedtls_x509_crt_parse failed to parse %d certificates\n\n", ret );
            mbedtls_x509_crt_free( &crt );
            goto exit;
        }

        mbedtls_printf( " ok\n" );

        /*
         * 1.2 Print the certificate(s)
         */
        while( cur != NULL )
        {
            mbedtls_printf( "  . Peer certificate information    ...\n" );
            ret = mbedtls_x509_crt_info( (char *) buf, sizeof( buf ) - 1, "      ",
                                 cur );
            if( ret == -1 )
            {
                mbedtls_printf( " failed\n  !  mbedtls_x509_crt_info returned %d\n\n", ret );
                mbedtls_x509_crt_free( &crt );
                goto exit;
            }

            mbedtls_printf( "%s\n", buf );

            cur = cur->next;
        }

        ret = 0;

        /*
         * 1.3 Verify the certificate
         */
        if( verify )
        {
            mbedtls_printf( "  . Verifying X.509 certificate..." );

            if( ( ret = mbedtls_x509_crt_verify( &crt, &cacert, &cacrl, NULL, &flags,
                                         my_verify, NULL ) ) != 0 )
            {
                mbedtls_printf( " failed\n" );

                if( ( ret & MBEDTLS_BADCERT_EXPIRED ) != 0 )
                    mbedtls_printf( "  ! server certificate has expired\n" );

                if( ( ret & MBEDTLS_X509_BADCERT_REVOKED ) != 0 )
                    mbedtls_printf( "  ! server certificate has been revoked\n" );

                if( ( ret & MBEDTLS_X509_BADCERT_CN_MISMATCH ) != 0 )
                    mbedtls_printf( "  ! CN mismatch (expected CN=%s)\n", opt.server_name );

                if( ( ret & MBEDTLS_X509_BADCERT_NOT_TRUSTED ) != 0 )
                    mbedtls_printf( "  ! self-signed or not signed by a trusted CA\n" );

                mbedtls_printf( "\n" );
            }
            else
                mbedtls_printf( " ok\n" );
        }

        mbedtls_x509_crt_free( &crt );
    }
    else if( opt.mode == MODE_SSL )
    {
        /*
         * 1. Initialize the RNG and the session data
         */
        mbedtls_printf( "\n  . Seeding the random number generator..." );
        fflush( stdout );

        mbedtls_entropy_init( &entropy );
        if( ( ret = mbedtls_ctr_drbg_init( &ctr_drbg, mbedtls_entropy_func, &entropy,
                                   (const unsigned char *) pers,
                                   strlen( pers ) ) ) != 0 )
        {
            mbedtls_printf( " failed\n  ! mbedtls_ctr_drbg_init returned %d\n", ret );
            goto exit;
        }

        mbedtls_printf( " ok\n" );

        /*
         * 2. Start the connection
         */
        mbedtls_printf( "  . SSL connection to tcp/%s/%-4d...", opt.server_name,
                                                        opt.server_port );
        fflush( stdout );

        if( ( ret = mbedtls_net_connect( &server_fd, opt.server_name,
                                 opt.server_port, MBEDTLS_NET_PROTO_TCP ) ) != 0 )
        {
            mbedtls_printf( " failed\n  ! mbedtls_net_connect returned %d\n\n", ret );
            goto exit;
        }

        /*
         * 3. Setup stuff
         */
        if( ( ret = mbedtls_ssl_init( &ssl ) ) != 0 )
        {
            mbedtls_printf( " failed\n  ! mbedtls_ssl_init returned %d\n\n", ret );
            goto exit;
        }

        mbedtls_ssl_set_endpoint( &ssl, MBEDTLS_SSL_IS_CLIENT );
        if( verify )
        {
            mbedtls_ssl_set_authmode( &ssl, MBEDTLS_SSL_VERIFY_REQUIRED );
            mbedtls_ssl_set_ca_chain( &ssl, &cacert, NULL, opt.server_name );
            mbedtls_ssl_set_verify( &ssl, my_verify, NULL );
        }
        else
            mbedtls_ssl_set_authmode( &ssl, MBEDTLS_SSL_VERIFY_NONE );

        mbedtls_ssl_set_rng( &ssl, mbedtls_ctr_drbg_random, &ctr_drbg );
        mbedtls_ssl_set_dbg( &ssl, my_debug, stdout );
        mbedtls_ssl_set_bio_timeout( &ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL, 0 );

        if( ( ret = mbedtls_ssl_set_own_cert( &ssl, &clicert, &pkey ) ) != 0 )
        {
            mbedtls_printf( " failed\n  ! mbedtls_ssl_set_own_cert returned %d\n\n", ret );
            goto exit;
        }

#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
        if( ( ret = mbedtls_ssl_set_hostname( &ssl, opt.server_name ) ) != 0 )
        {
            mbedtls_printf( " failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret );
            goto exit;
        }
#endif

        /*
         * 4. Handshake
         */
        while( ( ret = mbedtls_ssl_handshake( &ssl ) ) != 0 )
        {
            if( ret != MBEDTLS_ERR_NET_WANT_READ && ret != MBEDTLS_ERR_NET_WANT_WRITE )
            {
                mbedtls_printf( " failed\n  ! mbedtls_ssl_handshake returned %d\n\n", ret );
                mbedtls_ssl_free( &ssl );
                goto exit;
            }
        }

        mbedtls_printf( " ok\n" );

        /*
         * 5. Print the certificate
         */
        mbedtls_printf( "  . Peer certificate information    ...\n" );
        ret = mbedtls_x509_crt_info( (char *) buf, sizeof( buf ) - 1, "      ",
                             ssl.session->peer_cert );
        if( ret == -1 )
        {
            mbedtls_printf( " failed\n  !  mbedtls_x509_crt_info returned %d\n\n", ret );
            mbedtls_ssl_free( &ssl );
            goto exit;
        }

        mbedtls_printf( "%s\n", buf );

        mbedtls_ssl_close_notify( &ssl );
        mbedtls_ssl_free( &ssl );
    }
    else
        goto usage;

exit:

    if( server_fd )
        mbedtls_net_close( server_fd );
    mbedtls_x509_crt_free( &cacert );
    mbedtls_x509_crt_free( &clicert );
#if defined(MBEDTLS_X509_CRL_PARSE_C)
    mbedtls_x509_crl_free( &cacrl );
#endif
    mbedtls_pk_free( &pkey );
    mbedtls_ctr_drbg_free( &ctr_drbg );
    mbedtls_entropy_free( &entropy );

#if defined(_WIN32)
    mbedtls_printf( "  + Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    if( ret < 0 )
        ret = 1;

    return( ret );
}
#endif /* MBEDTLS_BIGNUM_C && MBEDTLS_ENTROPY_C && MBEDTLS_SSL_TLS_C &&
          MBEDTLS_SSL_CLI_C && MBEDTLS_NET_C && MBEDTLS_RSA_C &&
          MBEDTLS_X509_CRT_PARSE_C && MBEDTLS_FS_IO && MBEDTLS_CTR_DRBG_C */
