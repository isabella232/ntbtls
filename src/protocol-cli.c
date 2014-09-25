/* protocol-cli.c - TLS 1.2 client side protocol
 * Copyright (C) 2006-2014, Brainspark B.V.
 * Copyright (C) 2014 g10 code GmbH
 *
 * This file is part of NTBTLS
 *
 * NTBTLS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * NTBTLS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * This file was part of PolarSSL (http://www.polarssl.org).  Former
 * Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>.
 * Please do not file bug reports to them but to the address given in
 * the file AUTHORS in the top directory of NTBTLS.
 */

#include <config.h>
#include <stdlib.h>

#include "ntbtls-int.h"


static void
write_hostname_ext (ntbtls_t ssl, unsigned char *buf, size_t * olen)
{
  unsigned char *p = buf;

  *olen = 0;

  if (ssl->hostname == NULL)
    return;

  debug_msg (3, "client hello, adding server name extension: %s",
             ssl->hostname);

  /*
   * struct {
   *     NameType name_type;
   *     select (name_type) {
   *         case host_name: HostName;
   *     } name;
   * } ServerName;
   *
   * enum {
   *     host_name(0), (255)
   * } NameType;
   *
   * opaque HostName<1..2^16-1>;
   *
   * struct {
   *     ServerName server_name_list<1..2^16-1>
   * } ServerNameList;
   */
  *p++ = (unsigned char) ((TLS_EXT_SERVERNAME >> 8) & 0xFF);
  *p++ = (unsigned char) ((TLS_EXT_SERVERNAME) & 0xFF);

  *p++ = (unsigned char) (((ssl->hostname_len + 5) >> 8) & 0xFF);
  *p++ = (unsigned char) (((ssl->hostname_len + 5)) & 0xFF);

  *p++ = (unsigned char) (((ssl->hostname_len + 3) >> 8) & 0xFF);
  *p++ = (unsigned char) (((ssl->hostname_len + 3)) & 0xFF);

  *p++ = (unsigned char) ((TLS_EXT_SERVERNAME) & 0xFF);
  *p++ = (unsigned char) ((ssl->hostname_len >> 8) & 0xFF);
  *p++ = (unsigned char) ((ssl->hostname_len) & 0xFF);

  memcpy (p, ssl->hostname, ssl->hostname_len);

  *olen = ssl->hostname_len + 9;
}


static void
write_cli_renegotiation_ext (ntbtls_t ssl,
                             unsigned char *buf, size_t * olen)
{
  unsigned char *p = buf;

  *olen = 0;

  if (ssl->renegotiation != TLS_RENEGOTIATION)
    return;

  debug_msg (3, "client hello, adding renegotiation extension");

  /*
   * Secure renegotiation
   */
  *p++ = (unsigned char) ((TLS_EXT_RENEGOTIATION_INFO >> 8) & 0xFF);
  *p++ = (unsigned char) ((TLS_EXT_RENEGOTIATION_INFO) & 0xFF);

  *p++ = 0x00;
  *p++ = (ssl->verify_data_len + 1) & 0xFF;
  *p++ = ssl->verify_data_len & 0xFF;

  memcpy (p, ssl->own_verify_data, ssl->verify_data_len);

  *olen = 5 + ssl->verify_data_len;
}


static void
write_signature_algorithms_ext (ntbtls_t ssl,
                                unsigned char *buf, size_t * olen)
{
  unsigned char *p = buf;
  size_t sig_alg_len = 0;
  unsigned char *sig_alg_list = buf + 6;

  *olen = 0;

  if (ssl->max_minor_ver != TLS_MINOR_VERSION_3)
    return;

  debug_msg (3, "client hello, adding signature_algorithms extension");

  /*
   * Prepare signature_algorithms extension (TLS 1.2)
   */
  sig_alg_list[sig_alg_len++] = TLS_HASH_SHA512;
  sig_alg_list[sig_alg_len++] = TLS_SIG_RSA;
  sig_alg_list[sig_alg_len++] = TLS_HASH_SHA384;
  sig_alg_list[sig_alg_len++] = TLS_SIG_RSA;
  sig_alg_list[sig_alg_len++] = TLS_HASH_SHA256;
  sig_alg_list[sig_alg_len++] = TLS_SIG_RSA;
  sig_alg_list[sig_alg_len++] = TLS_HASH_SHA224;
  sig_alg_list[sig_alg_len++] = TLS_SIG_RSA;
  sig_alg_list[sig_alg_len++] = TLS_HASH_SHA1;
  sig_alg_list[sig_alg_len++] = TLS_SIG_RSA;
  sig_alg_list[sig_alg_len++] = TLS_HASH_SHA512;
  sig_alg_list[sig_alg_len++] = TLS_SIG_ECDSA;
  sig_alg_list[sig_alg_len++] = TLS_HASH_SHA384;
  sig_alg_list[sig_alg_len++] = TLS_SIG_ECDSA;
  sig_alg_list[sig_alg_len++] = TLS_HASH_SHA256;
  sig_alg_list[sig_alg_len++] = TLS_SIG_ECDSA;
  sig_alg_list[sig_alg_len++] = TLS_HASH_SHA224;
  sig_alg_list[sig_alg_len++] = TLS_SIG_ECDSA;
  sig_alg_list[sig_alg_len++] = TLS_HASH_SHA1;
  sig_alg_list[sig_alg_len++] = TLS_SIG_ECDSA;


  /*
   * enum {
   *     none(0), md5(1), sha1(2), sha224(3), sha256(4), sha384(5),
   *     sha512(6), (255)
   * } HashAlgorithm;
   *
   * enum { anonymous(0), rsa(1), dsa(2), ecdsa(3), (255) }
   *   SignatureAlgorithm;
   *
   * struct {
   *     HashAlgorithm hash;
   *     SignatureAlgorithm signature;
   * } SignatureAndHashAlgorithm;
   *
   * SignatureAndHashAlgorithm
   *   supported_signature_algorithms<2..2^16-2>;
   */
  *p++ = (unsigned char) ((TLS_EXT_SIG_ALG >> 8) & 0xFF);
  *p++ = (unsigned char) ((TLS_EXT_SIG_ALG) & 0xFF);

  *p++ = (unsigned char) (((sig_alg_len + 2) >> 8) & 0xFF);
  *p++ = (unsigned char) (((sig_alg_len + 2)) & 0xFF);

  *p++ = (unsigned char) ((sig_alg_len >> 8) & 0xFF);
  *p++ = (unsigned char) ((sig_alg_len) & 0xFF);

  *olen = 6 + sig_alg_len;
}


static void
write_supported_elliptic_curves_ext (ntbtls_t ssl,
                                     unsigned char *buf, size_t * olen)
{
  unsigned char *p = buf;
  unsigned char *elliptic_curve_list = p + 6;
  size_t elliptic_curve_len = 0;
  const ecp_curve_info *info;
  const ecp_group_id *grp_id;

  *olen = 0;

  debug_msg (3, "client hello, adding supported_elliptic_curves extension");

  for (grp_id = ssl->curve_list; *grp_id != POLARSSL_ECP_DP_NONE; grp_id++)
    {
      info = ecp_curve_info_from_grp_id (*grp_id);
      elliptic_curve_list[elliptic_curve_len++] = info->tls_id >> 8;
      elliptic_curve_list[elliptic_curve_len++] = info->tls_id & 0xFF;
    }

  if (elliptic_curve_len == 0)
    return;

  *p++ = (unsigned char) ((TLS_EXT_SUPPORTED_ELLIPTIC_CURVES >> 8) & 0xFF);
  *p++ = (unsigned char) ((TLS_EXT_SUPPORTED_ELLIPTIC_CURVES) & 0xFF);

  *p++ = (unsigned char) (((elliptic_curve_len + 2) >> 8) & 0xFF);
  *p++ = (unsigned char) (((elliptic_curve_len + 2)) & 0xFF);

  *p++ = (unsigned char) (((elliptic_curve_len) >> 8) & 0xFF);
  *p++ = (unsigned char) (((elliptic_curve_len)) & 0xFF);

  *olen = 6 + elliptic_curve_len;
}


static void
write_cli_supported_point_formats_ext (ntbtls_t ssl,
                                       unsigned char *buf, size_t * olen)
{
  unsigned char *p = buf;
  ((void) ssl);

  *olen = 0;

  debug_msg (3, "client hello, adding supported_point_formats extension");

  *p++ = (unsigned char) ((TLS_EXT_SUPPORTED_POINT_FORMATS >> 8) & 0xFF);
  *p++ = (unsigned char) ((TLS_EXT_SUPPORTED_POINT_FORMATS) & 0xFF);

  *p++ = 0x00;
  *p++ = 2;

  *p++ = 1;
  *p++ = POLARSSL_ECP_PF_UNCOMPRESSED;

  *olen = 6;
}


static void
write_cli_max_fragment_length_ext (ntbtls_t ssl,
                                   unsigned char *buf, size_t * olen)
{
  unsigned char *p = buf;

  if (ssl->mfl_code == SSL_MAX_FRAG_LEN_NONE)
    {
      *olen = 0;
      return;
    }

  debug_msg (3, "client hello, adding max_fragment_length extension");

  *p++ = (unsigned char) ((TLS_EXT_MAX_FRAGMENT_LENGTH >> 8) & 0xFF);
  *p++ = (unsigned char) ((TLS_EXT_MAX_FRAGMENT_LENGTH) & 0xFF);

  *p++ = 0x00;
  *p++ = 1;

  *p++ = ssl->mfl_code;

  *olen = 5;
}


static void
write_cli_truncated_hmac_ext (ntbtls_t ssl,
                              unsigned char *buf, size_t * olen)
{
  unsigned char *p = buf;

  if (ssl->trunc_hmac == SSL_TRUNC_HMAC_DISABLED)
    {
      *olen = 0;
      return;
    }

  debug_msg (3, "client hello, adding truncated_hmac extension");

  *p++ = (unsigned char) ((TLS_EXT_TRUNCATED_HMAC >> 8) & 0xFF);
  *p++ = (unsigned char) ((TLS_EXT_TRUNCATED_HMAC) & 0xFF);

  *p++ = 0x00;
  *p++ = 0x00;

  *olen = 4;
}


static void
write_cli_session_ticket_ext (ntbtls_t ssl,
                              unsigned char *buf, size_t * olen)
{
  unsigned char *p = buf;
  size_t tlen = ssl->session_negotiate->ticket_len;

  if (ssl->session_tickets == SSL_SESSION_TICKETS_DISABLED)
    {
      *olen = 0;
      return;
    }

  debug_msg (3, "client hello, adding session ticket extension");

  *p++ = (unsigned char) ((TLS_EXT_SESSION_TICKET >> 8) & 0xFF);
  *p++ = (unsigned char) ((TLS_EXT_SESSION_TICKET) & 0xFF);

  *p++ = (unsigned char) ((tlen >> 8) & 0xFF);
  *p++ = (unsigned char) ((tlen) & 0xFF);

  *olen = 4;

  if (ssl->session_negotiate->ticket == NULL ||
      ssl->session_negotiate->ticket_len == 0)
    {
      return;
    }

  debug_msg (3, "sending session ticket of length %d", tlen);

  memcpy (p, ssl->session_negotiate->ticket, tlen);

  *olen += tlen;
}


static void
write_cli_alpn_ext (ntbtls_t ssl, unsigned char *buf, size_t * olen)
{
  unsigned char *p = buf;
  const char **cur;

  if (ssl->alpn_list == NULL)
    {
      *olen = 0;
      return;
    }

  debug_msg (3, "client hello, adding alpn extension");

  *p++ = (unsigned char) ((TLS_EXT_ALPN >> 8) & 0xFF);
  *p++ = (unsigned char) ((TLS_EXT_ALPN) & 0xFF);

  /*
   * opaque ProtocolName<1..2^8-1>;
   *
   * struct {
   *     ProtocolName protocol_name_list<2..2^16-1>
   * } ProtocolNameList;
   */

  /* Skip writing extension and list length for now */
  p += 4;

  for (cur = ssl->alpn_list; *cur != NULL; cur++)
    {
      *p = (unsigned char) (strlen (*cur) & 0xFF);
      memcpy (p + 1, *cur, *p);
      p += 1 + *p;
    }

  *olen = p - buf;

  /* List length = olen - 2 (ext_type) - 2 (ext_len) - 2 (list_len) */
  buf[4] = (unsigned char) (((*olen - 6) >> 8) & 0xFF);
  buf[5] = (unsigned char) (((*olen - 6)) & 0xFF);

  /* Extension length = olen - 2 (ext_type) - 2 (ext_len) */
  buf[2] = (unsigned char) (((*olen - 4) >> 8) & 0xFF);
  buf[3] = (unsigned char) (((*olen - 4)) & 0xFF);
}


static int
write_client_hello (ntbtls_t tls)
{
  int ret;
  size_t i, n, olen;
  size_t ext_len = 0;
  unsigned char *buf;
  unsigned char *p, *q;
  time_t t;
  const int *ciphersuites;
  const ssl_ciphersuite_t *ciphersuite_info;

  debug_msg (2, "=> write client hello");

  if (tls->renegotiation == TLS_INITIAL_HANDSHAKE)
    {
      tls->major_ver = tls->min_major_ver;
      tls->minor_ver = tls->min_minor_ver;
    }

  if (tls->max_major_ver == 0 && tls->max_minor_ver == 0)
    {
      tls->max_major_ver = TLS_MAX_MAJOR_VERSION;
      tls->max_minor_ver = TLS_MAX_MINOR_VERSION;
    }

  /*
   *     0  .   0   handshake type
   *     1  .   3   handshake length
   *     4  .   5   highest version supported
   *     6  .   9   current UNIX time
   *    10  .  37   random bytes
   */
  buf = tls->out_msg;
  p = buf + 4;

  *p++ = (unsigned char) tls->max_major_ver;
  *p++ = (unsigned char) tls->max_minor_ver;

  debug_msg (3, "client hello, max version: [%d:%d]", buf[4], buf[5]);

  t = time (NULL);
  *p++ = (unsigned char) (t >> 24);
  *p++ = (unsigned char) (t >> 16);
  *p++ = (unsigned char) (t >> 8);
  *p++ = (unsigned char) (t);

  debug_msg (3, "client hello, current time: %lu", t);

  //FIXME: Check RNG requirements.
  gcry_create_nonce (p, 28);
  p += 28;

  memcpy (tls->handshake->randbytes, buf + 6, 32);

  debug_buf (3, "client hello, random bytes", buf + 6, 32);

  /*
   *    38  .  38   session id length
   *    39  . 39+n  session id
   *   40+n . 41+n  ciphersuitelist length
   *   42+n . ..    ciphersuitelist
   *   ..   . ..    compression methods length
   *   ..   . ..    compression methods
   *   ..   . ..    extensions length
   *   ..   . ..    extensions
   */
  n = tls->session_negotiate->length;

  if (tls->renegotiation != TLS_INITIAL_HANDSHAKE || n < 16 || n > 32 ||
      tls->handshake->resume == 0)
    {
      n = 0;
    }

  /*
   * RFC 5077 section 3.4: "When presenting a ticket, the client MAY
   * generate and include a Session ID in the TLS ClientHello."
   */
  if (tls->renegotiation == SSL_INITIAL_HANDSHAKE &&
      tls->session_negotiate->ticket != NULL &&
      tls->session_negotiate->ticket_len != 0)
    {
      ret = tls->f_rng (tls->p_rng, tls->session_negotiate->id, 32);

      if (ret != 0)
        return (ret);

      tls->session_negotiate->length = n = 32;
    }

  *p++ = (unsigned char) n;

  for (i = 0; i < n; i++)
    *p++ = tls->session_negotiate->id[i];

  debug_msg (3, "client hello, session id len.: %d", n);
  debug_buf (3, "client hello, session id", buf + 39, n);

  ciphersuites = tls->ciphersuite_list[tls->minor_ver];
  n = 0;
  q = p;

  /* Skip writing ciphersuite length for now.  */
  p += 2;

  /*
   * Add TLS_EMPTY_RENEGOTIATION_INFO_SCSV
   */
  if (tls->renegotiation == SSL_INITIAL_HANDSHAKE)
    {
      *p++ = (unsigned char) (SSL_EMPTY_RENEGOTIATION_INFO >> 8);
      *p++ = (unsigned char) (SSL_EMPTY_RENEGOTIATION_INFO);
      n++;
    }

  for (i = 0; ciphersuites[i] != 0; i++)
    {
      ciphersuite_info = ssl_ciphersuite_from_id (ciphersuites[i]);

      if (ciphersuite_info == NULL)
        continue;

      if (ciphersuite_info->min_minor_ver > tls->max_minor_ver ||
          ciphersuite_info->max_minor_ver < tls->min_minor_ver)
        continue;

      debug_msg (3, "client hello, add ciphersuite: %2d",
                 ciphersuites[i]);

      n++;
      *p++ = (unsigned char) (ciphersuites[i] >> 8);
      *p++ = (unsigned char) (ciphersuites[i]);
    }

  *q++ = (unsigned char) (n >> 7);
  *q++ = (unsigned char) (n << 1);

  debug_msg (3, "client hello, got %d ciphersuites", n);

  debug_msg (3, "client hello, compress len.: %d", 2);
  debug_msg (3, "client hello, compress alg.: %d %d",
             SSL_COMPRESS_DEFLATE, SSL_COMPRESS_NULL);

  *p++ = 2;
  *p++ = SSL_COMPRESS_DEFLATE;
  *p++ = SSL_COMPRESS_NULL;

  /* First write extensions, then the total length.  */
  write_hostname_ext (ssl, p + 2 + ext_len, &olen);
  ext_len += olen;

  write_cli_renegotiation_ext (ssl, p + 2 + ext_len, &olen);
  ext_len += olen;

  write_signature_algorithms_ext (ssl, p + 2 + ext_len, &olen);
  ext_len += olen;

  ssl_write_supported_elliptic_curves_ext (ssl, p + 2 + ext_len, &olen);
  ext_len += olen;

  write_cli_supported_point_formats_ext (ssl, p + 2 + ext_len, &olen);
  ext_len += olen;

  write_cli_max_fragment_length_ext (ssl, p + 2 + ext_len, &olen);
  ext_len += olen;

  write_cli_truncated_hmac_ext (ssl, p + 2 + ext_len, &olen);
  ext_len += olen;

  write_cli_session_ticket_ext (ssl, p + 2 + ext_len, &olen);
  ext_len += olen;

  write_cli_alpn_ext (ssl, p + 2 + ext_len, &olen);
  ext_len += olen;

  debug_msg (3, "client hello, total extension length: %d", ext_len);

  if (ext_len > 0)
    {
      *p++ = (unsigned char) ((ext_len >> 8) & 0xFF);
      *p++ = (unsigned char) ((ext_len) & 0xFF);
      p += ext_len;
    }

  tls->out_msglen = p - buf;
  tls->out_msgtype = TLS_MSG_HANDSHAKE;
  tls->out_msg[0] = TLS_HS_CLIENT_HELLO;

  tls->state++;

  err = _ntbtls_write_record (tls);
  if (err)
    {
      SSL_DEBUG_RET (1, "ssl_write_record", ret);
      return (ret);
    }

  debug_msg (2, "<= write client hello");

  return (0);
}

static int
ssl_parse_renegotiation_info (ntbtls_t ssl,
                              const unsigned char *buf, size_t len)
{
  int ret;

  if (ssl->renegotiation == SSL_INITIAL_HANDSHAKE)
    {
      if (len != 1 || buf[0] != 0x0)
        {
          debug_msg (1, "non-zero length renegotiated connection field");

          if ((ret = ssl_send_fatal_handshake_failure (ssl)) != 0)
            return (ret);

          return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
        }

      ssl->secure_renegotiation = SSL_SECURE_RENEGOTIATION;
    }
  else
    {
      /* Check verify-data in constant-time. The length OTOH is no secret */
      if (len != 1 + ssl->verify_data_len * 2 ||
          buf[0] != ssl->verify_data_len * 2 ||
          safer_memcmp (buf + 1,
                        ssl->own_verify_data, ssl->verify_data_len) != 0 ||
          safer_memcmp (buf + 1 + ssl->verify_data_len,
                        ssl->peer_verify_data, ssl->verify_data_len) != 0)
        {
          debug_msg (1, "non-matching renegotiated connection field");

          if ((ret = ssl_send_fatal_handshake_failure (ssl)) != 0)
            return (ret);

          return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
        }
    }

  return (0);
}


static int
ssl_parse_max_fragment_length_ext (ntbtls_t ssl,
                                   const unsigned char *buf, size_t len)
{
  /*
   * server should use the extension only if we did,
   * and if so the server's value should match ours (and len is always 1)
   */
  if (ssl->mfl_code == SSL_MAX_FRAG_LEN_NONE ||
      len != 1 || buf[0] != ssl->mfl_code)
    {
      return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
    }

  return (0);
}



static int
ssl_parse_truncated_hmac_ext (ntbtls_t ssl,
                              const unsigned char *buf, size_t len)
{
  if (ssl->trunc_hmac == SSL_TRUNC_HMAC_DISABLED || len != 0)
    {
      return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
    }

  ((void) buf);

  ssl->session_negotiate->trunc_hmac = SSL_TRUNC_HMAC_ENABLED;

  return (0);
}


static int
ssl_parse_session_ticket_ext (ntbtls_t ssl,
                              const unsigned char *buf, size_t len)
{
  if (ssl->session_tickets == SSL_SESSION_TICKETS_DISABLED || len != 0)
    {
      return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
    }

  ((void) buf);

  ssl->handshake->new_session_ticket = 1;

  return (0);
}


static int
ssl_parse_supported_point_formats_ext (ntbtls_t ssl,
                                       const unsigned char *buf, size_t len)
{
  size_t list_size;
  const unsigned char *p;

  list_size = buf[0];
  if (list_size + 1 != len)
    {
      debug_msg (1, "bad server hello message");
      return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
    }

  p = buf + 1;
  while (list_size > 0)
    {
      if (p[0] == POLARSSL_ECP_PF_UNCOMPRESSED ||
          p[0] == POLARSSL_ECP_PF_COMPRESSED)
        {
          ssl->handshake->ecdh_ctx.point_format = p[0];
          debug_msg (4, "point format selected: %d", p[0]);
          return (0);
        }

      list_size--;
      p++;
    }

  debug_msg (1, "no point format in common");
  return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
}


static int
ssl_parse_alpn_ext (ntbtls_t ssl, const unsigned char *buf, size_t len)
{
  size_t list_len, name_len;
  const char **p;

  /* If we didn't send it, the server shouldn't send it */
  if (ssl->alpn_list == NULL)
    return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);

  /*
   * opaque ProtocolName<1..2^8-1>;
   *
   * struct {
   *     ProtocolName protocol_name_list<2..2^16-1>
   * } ProtocolNameList;
   *
   * the "ProtocolNameList" MUST contain exactly one "ProtocolName"
   */

  /* Min length is 2 (list_len) + 1 (name_len) + 1 (name) */
  if (len < 4)
    return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);

  list_len = (buf[0] << 8) | buf[1];
  if (list_len != len - 2)
    return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);

  name_len = buf[2];
  if (name_len != list_len - 1)
    return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);

  /* Check that the server chosen protocol was in our list and save it */
  for (p = ssl->alpn_list; *p != NULL; p++)
    {
      if (name_len == strlen (*p) && memcmp (buf + 3, *p, name_len) == 0)
        {
          ssl->alpn_chosen = *p;
          return (0);
        }
    }

  return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
}


static int
parse_server_hello (ntbtls_t ssl)
{
  int ret, i, comp;
  size_t n;
  size_t ext_len = 0;
  unsigned char *buf, *ext;
  int renegotiation_info_seen = 0;
  int handshake_failure = 0;
  uint32_t t;

  debug_msg (2, "=> parse server hello");

  /*
   *     0  .   0   handshake type
   *     1  .   3   handshake length
   *     4  .   5   protocol version
   *     6  .   9   UNIX time()
   *    10  .  37   random bytes
   */
  buf = ssl->in_msg;

  if ((ret = _ntbtls_read_record (ssl)) != 0)
    {
      debug_ret (1, "read_record", ret);
      return ret;
    }

  if (ssl->in_msgtype != TLS_MSG_HANDSHAKE)
    {
      debug_msg (1, "bad server hello message");
      return gpg_error (GPG_ERR_UNEXPECTED_MSG);
    }

  debug_msg (3, "server hello, chosen version: [%d:%d]",  buf[4], buf[5]);

  if (ssl->in_hslen < 42 ||
      buf[0] != TLS_HS_SERVER_HELLO || buf[4] != SSL_MAJOR_VERSION_3)
    {
      debug_msg (1, "bad server hello message");
      return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
    }

  if (buf[5] > ssl->max_minor_ver)
    {
      debug_msg (1, "bad server hello message");
      return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
    }

  ssl->minor_ver = buf[5];

  if (ssl->minor_ver < ssl->min_minor_ver)
    {
      debug_msg (1, "server only supports ssl smaller than minimum"
                 " [%d:%d] < [%d:%d]", ssl->major_ver,
                 ssl->minor_ver, buf[4], buf[5]);

      ssl_send_alert_message (ssl, TLS_ALERT_LEVEL_FATAL,
                              TLS_ALERT_MSG_PROTOCOL_VERSION);

      return gpg_error (GPG_ERR_UNSUPPORTED_PROTOCOL);
    }

  t = ((uint32_t) buf[6] << 24)
    | ((uint32_t) buf[7] << 16)
    | ((uint32_t) buf[8] << 8) | ((uint32_t) buf[9]);
  debug_msg (3, "server hello, current time: %lu", t);

  memcpy (ssl->handshake->randbytes + 32, buf + 6, 32);

  n = buf[38];

  debug_buf (3, "server hello, random bytes", buf + 6, 32);

  if (n > 32)
    {
      debug_msg (1, "bad server hello message");
      return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
    }

  /*
   *    38  .  38   session id length
   *    39  . 38+n  session id
   *   39+n . 40+n  chosen ciphersuite
   *   41+n . 41+n  chosen compression alg.
   *   42+n . 43+n  extensions length
   *   44+n . 44+n+m extensions
   */
  if (ssl->in_hslen > 42 + n)
    {
      ext_len = ((buf[42 + n] << 8) | (buf[43 + n]));

      if ((ext_len > 0 && ext_len < 4) || ssl->in_hslen != 44 + n + ext_len)
        {
          debug_msg (1, "bad server hello message");
          return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
        }
    }

  i = (buf[39 + n] << 8) | buf[40 + n];
  comp = buf[41 + n];

  /*
   * Initialize update checksum functions
   */
  ssl->transform_negotiate->ciphersuite_info = ssl_ciphersuite_from_id (i);

  if (ssl->transform_negotiate->ciphersuite_info == NULL)
    {
      debug_msg (1, "ciphersuite info for %04x not found", i);
      return gpg_error (GPG_ERR_INV_ARG);
    }

  ssl_optimize_checksum (ssl, ssl->transform_negotiate->ciphersuite_info);

  debug_msg (3, "server hello, session id len.: %d", n);
  debug_buf (3, "server hello, session id", buf + 39, n);

  /*
   * Check if the session can be resumed
   */
  if (ssl->renegotiation != SSL_INITIAL_HANDSHAKE ||
      ssl->handshake->resume == 0 || n == 0 ||
      ssl->session_negotiate->ciphersuite != i ||
      ssl->session_negotiate->compression != comp ||
      ssl->session_negotiate->length != n ||
      memcmp (ssl->session_negotiate->id, buf + 39, n) != 0)
    {
      ssl->state++;
      ssl->handshake->resume = 0;
      ssl->session_negotiate->start = time (NULL);
      ssl->session_negotiate->ciphersuite = i;
      ssl->session_negotiate->compression = comp;
      ssl->session_negotiate->length = n;
      memcpy (ssl->session_negotiate->id, buf + 39, n);
    }
  else
    {
      ssl->state = SSL_SERVER_CHANGE_CIPHER_SPEC;

      if ((ret = _ntbtls_derive_keys (ssl)) != 0)
        {
          SSL_DEBUG_RET (1, "ssl_derive_keys", ret);
          return (ret);
        }
    }

  debug_msg (3, "%s session has been resumed",
             ssl->handshake->resume ? "a" : "no");

  debug_msg (3, "server hello, chosen ciphersuite: %d", i);
  debug_msg (3, "server hello, compress alg.: %d", buf[41 + n]);

  i = 0;
  while (1)
    {
      if (ssl->ciphersuite_list[ssl->minor_ver][i] == 0)
        {
          debug_msg (1, "bad server hello message");
          return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
        }

      if (ssl->ciphersuite_list[ssl->minor_ver][i++] ==
          ssl->session_negotiate->ciphersuite)
        {
          break;
        }
    }

  if (comp != TLS_COMPRESS_NULL
      && comp != TLS_COMPRESS_DEFLATE
    )
    {
      debug_msg (1, "bad server hello message");
      return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
    }
  ssl->session_negotiate->compression = comp;

  ext = buf + 44 + n;

  debug_msg (2, "server hello, total extension length: %d", ext_len);

  while (ext_len)
    {
      unsigned int ext_id = ((ext[0] << 8) | (ext[1]));
      unsigned int ext_size = ((ext[2] << 8) | (ext[3]));

      if (ext_size + 4 > ext_len)
        {
          debug_msg (1, "bad server hello message");
          return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
        }

      switch (ext_id)
        {
        case TLS_EXT_RENEGOTIATION_INFO:
          debug_msg (3, "found renegotiation extension");
          renegotiation_info_seen = 1;

          if ((ret = ssl_parse_renegotiation_info (ssl, ext + 4,
                                                   ext_size)) != 0)
            return (ret);

          break;

        case TLS_EXT_MAX_FRAGMENT_LENGTH:
          debug_msg (3, "found max_fragment_length extension");

          if ((ret = ssl_parse_max_fragment_length_ext (ssl,
                                                        ext + 4,
                                                        ext_size)) != 0)
            {
              return (ret);
            }

          break;

        case TLS_EXT_TRUNCATED_HMAC:
          debug_msg (3, "found truncated_hmac extension");

          if ((ret = ssl_parse_truncated_hmac_ext (ssl,
                                                   ext + 4, ext_size)) != 0)
            {
              return (ret);
            }

          break;

        case TLS_EXT_SESSION_TICKET:
          debug_msg (3, "found session_ticket extension");

          if ((ret = ssl_parse_session_ticket_ext (ssl,
                                                   ext + 4, ext_size)) != 0)
            {
              return (ret);
            }

          break;

        case TLS_EXT_SUPPORTED_POINT_FORMATS:
          debug_msg (3, "found supported_point_formats extension");

          if ((ret = ssl_parse_supported_point_formats_ext (ssl,
                                                            ext + 4,
                                                            ext_size)) != 0)
            {
              return (ret);
            }

          break;

        case TLS_EXT_ALPN:
          debug_msg (3, "found alpn extension");

          if ((ret = ssl_parse_alpn_ext (ssl, ext + 4, ext_size)) != 0)
            return (ret);

          break;

        default:
          debug_msg (3, "unknown extension found: %d (ignoring)", ext_id);
          break;
        }

      ext_len -= 4 + ext_size;
      ext += 4 + ext_size;

      if (ext_len > 0 && ext_len < 4)
        {
          debug_msg (1, "bad server hello message");
          return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
        }
    }

  /*
   * Renegotiation security checks
   */
  if (ssl->secure_renegotiation == TLS_LEGACY_RENEGOTIATION &&
      ssl->allow_legacy_renegotiation == TLS_LEGACY_BREAK_HANDSHAKE)
    {
      debug_msg (1, "legacy renegotiation, breaking off handshake");
      handshake_failure = 1;
    }
  else if (ssl->renegotiation == TLS_RENEGOTIATION &&
           ssl->secure_renegotiation == TLS_SECURE_RENEGOTIATION &&
           renegotiation_info_seen == 0)
    {
      debug_msg (1, "renegotiation_info extension missing (secure)");
      handshake_failure = 1;
    }
  else if (ssl->renegotiation == TLS_RENEGOTIATION &&
           ssl->secure_renegotiation == TLS_LEGACY_RENEGOTIATION &&
           ssl->allow_legacy_renegotiation == TLS_LEGACY_NO_RENEGOTIATION)
    {
      debug_msg (1, "legacy renegotiation not allowed");
      handshake_failure = 1;
    }
  else if (ssl->renegotiation == TLS_RENEGOTIATION &&
           ssl->secure_renegotiation == TLS_LEGACY_RENEGOTIATION &&
           renegotiation_info_seen == 1)
    {
      debug_msg (1, "renegotiation_info extension present (legacy)");
      handshake_failure = 1;
    }

  if (handshake_failure == 1)
    {
      if ((ret = ssl_send_fatal_handshake_failure (ssl)) != 0)
        return (ret);

      return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO);
    }

  debug_msg (2, "<= parse server hello");

  return (0);
}


static int
ssl_parse_server_dh_params (ntbtls_t ssl, unsigned char **p,
                            unsigned char *end)
{
  int ret = gpg_error (GPG_ERR_NOT_IMPLEMENTED);

  /*
   * Ephemeral DH parameters:
   *
   * struct {
   *     opaque dh_p<1..2^16-1>;
   *     opaque dh_g<1..2^16-1>;
   *     opaque dh_Ys<1..2^16-1>;
   * } ServerDHParams;
   */
  if ((ret = dhm_read_params (&ssl->handshake->dhm_ctx, p, end)) != 0)
    {
      SSL_DEBUG_RET (2, ("dhm_read_params"), ret);
      return (ret);
    }

  if (ssl->handshake->dhm_ctx.len < 64 || ssl->handshake->dhm_ctx.len > 512)
    {
      debug_msg (1, "bad server key exchange message (DHM length)");
      return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
    }

  SSL_DEBUG_MPI (3, "DHM: P ", &ssl->handshake->dhm_ctx.P);
  SSL_DEBUG_MPI (3, "DHM: G ", &ssl->handshake->dhm_ctx.G);
  SSL_DEBUG_MPI (3, "DHM: GY", &ssl->handshake->dhm_ctx.GY);

  return (ret);
}


static int
ssl_check_server_ecdh_params (const ntbtls_t ssl)
{
  const ecp_curve_info *curve_info;

  curve_info = ecp_curve_info_from_grp_id (ssl->handshake->ecdh_ctx.grp.id);
  if (curve_info == NULL)
    {
      debug_bug ();
      return gpg_error (GPG_ERR_INTERNAL);
    }

  debug_msg (2, "ECDH curve: %s", curve_info->name);

  if (!ssl_curve_is_acceptable (ssl, ssl->handshake->ecdh_ctx.grp.id))
    return (-1);

  SSL_DEBUG_ECP (3, "ECDH: Qp", &ssl->handshake->ecdh_ctx.Qp);

  return (0);
}


static int
ssl_parse_server_ecdh_params (ntbtls_t ssl,
                              unsigned char **p, unsigned char *end)
{
  int ret = gpg_error (GPG_ERR_NOT_IMPLEMENTED);

  /*
   * Ephemeral ECDH parameters:
   *
   * struct {
   *     ECParameters curve_params;
   *     ECPoint      public;
   * } ServerECDHParams;
   */
  if ((ret = ecdh_read_params (&ssl->handshake->ecdh_ctx,
                               (const unsigned char **) p, end)) != 0)
    {
      SSL_DEBUG_RET (1, ("ecdh_read_params"), ret);
      return (ret);
    }

  if (ssl_check_server_ecdh_params (ssl) != 0)
    {
      debug_msg (1, "bad server key exchange message (ECDHE curve)");
      return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
    }

  return (ret);
}


static int
ssl_parse_server_psk_hint (ntbtls_t ssl,
                           unsigned char **p, unsigned char *end)
{
  int ret = gpg_error (GPG_ERR_NOT_IMPLEMENTED);
  size_t len;
  ((void) ssl);

  /*
   * PSK parameters:
   *
   * opaque psk_identity_hint<0..2^16-1>;
   */
  len = (*p)[0] << 8 | (*p)[1];
  *p += 2;

  if ((*p) + len > end)
    {
      debug_msg (1, "bad server key exchange message"
                 " (psk_identity_hint length)");
      return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
    }

  // TODO: Retrieve PSK identity hint and callback to app
  //
  *p += len;
  ret = 0;

  return (ret);
}


/*
 * Generate a pre-master secret and encrypt it with the server's RSA key
 */
static int
ssl_write_encrypted_pms (ntbtls_t ssl,
                         size_t offset, size_t * olen, size_t pms_offset)
{
  int ret;
  size_t len_bytes = ssl->minor_ver == SSL_MINOR_VERSION_0 ? 0 : 2;
  unsigned char *p = ssl->handshake->premaster + pms_offset;

  /*
   * Generate (part of) the pre-master as
   *  struct {
   *      ProtocolVersion client_version;
   *      opaque random[46];
   *  } PreMasterSecret;
   */
  p[0] = (unsigned char) ssl->max_major_ver;
  p[1] = (unsigned char) ssl->max_minor_ver;

  if ((ret = ssl->f_rng (ssl->p_rng, p + 2, 46)) != 0)
    {
      SSL_DEBUG_RET (1, "f_rng", ret);
      return (ret);
    }

  ssl->handshake->pmslen = 48;

  /*
   * Now write it out, encrypted
   */
  if (!pk_can_do (&ssl->session_negotiate->peer_cert->pk, POLARSSL_PK_RSA))
    {
      debug_msg (1, "certificate key type mismatch");
      return gpg_error (GPG_ERR_WRONG_PUBKEY_ALGO);
    }

  if ((ret = pk_encrypt (&ssl->session_negotiate->peer_cert->pk,
                         p, ssl->handshake->pmslen,
                         ssl->out_msg + offset + len_bytes, olen,
                         SSL_MAX_CONTENT_LEN - offset - len_bytes,
                         ssl->f_rng, ssl->p_rng)) != 0)
    {
      SSL_DEBUG_RET (1, "rsa_pkcs1_encrypt", ret);
      return (ret);
    }

  if (len_bytes == 2)
    {
      ssl->out_msg[offset + 0] = (unsigned char) (*olen >> 8);
      ssl->out_msg[offset + 1] = (unsigned char) (*olen);
      *olen += 2;
    }


  return (0);
}


static int
ssl_parse_signature_algorithm (ntbtls_t ssl,
                               unsigned char **p,
                               unsigned char *end,
                               md_type_t * md_alg, pk_type_t * pk_alg)
{
  ((void) ssl);
  *md_alg = POLARSSL_MD_NONE;
  *pk_alg = POLARSSL_PK_NONE;

  /* Only in TLS 1.2 */
  if (ssl->minor_ver != SSL_MINOR_VERSION_3)
    {
      return (0);
    }

  if ((*p) + 2 > end)
    return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);

  /*
   * Get hash algorithm
   */
  *md_alg = _ntbtls_md_alg_from_hash ((*p)[0]);
  if (!*md_alg)
    {
      debug_msg (2, "Server used unsupported HashAlgorithm %d", *(p)[0]);
      return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
    }

  /*
   * Get signature algorithm
   */
  *pk_alg = _ntbtls_pk_alg_from_sig ((*p)[1]);
  if (!*pk_alg)
    {
      debug_msg (2, "server used unsupported SignatureAlgorithm %d", (*p)[1]);
      return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
    }

  debug_msg (2, "Server used SignatureAlgorithm %d", (*p)[1]);
  debug_msg (2, "Server used HashAlgorithm %d", (*p)[0]);
  *p += 2;

  return (0);
}


static int
ssl_get_ecdh_params_from_cert (ntbtls_t ssl)
{
  int ret;
  const ecp_keypair *peer_key;

  if (!pk_can_do (&ssl->session_negotiate->peer_cert->pk, POLARSSL_PK_ECKEY))
    {
      debug_msg (1, "server key not ECDH capable");
      return gpg_error (GPG_ERR_WRONG_PUBKEY_ALGO);
    }

  peer_key = pk_ec (ssl->session_negotiate->peer_cert->pk);

  if ((ret = ecdh_get_params (&ssl->handshake->ecdh_ctx, peer_key,
                              POLARSSL_ECDH_THEIRS)) != 0)
    {
      SSL_DEBUG_RET (1, ("ecdh_get_params"), ret);
      return (ret);
    }

  if (ssl_check_server_ecdh_params (ssl) != 0)
    {
      debug_msg (1, "bad server certificate (ECDH curve)");
      return gpg_error (GPG_ERR_BAD_HS_CERT);
    }

  return (ret);
}


static int
parse_server_key_exchange (ntbtls_t ssl)
{
  int ret;
  const ssl_ciphersuite_t *ciphersuite_info =
    ssl->transform_negotiate->ciphersuite_info;
  unsigned char *p, *end;
  size_t sig_len, params_len;
  unsigned char hash[64];
  md_type_t md_alg = POLARSSL_MD_NONE;
  size_t hashlen;
  pk_type_t pk_alg = POLARSSL_PK_NONE;


  debug_msg (2, "=> parse server key exchange");

  if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA)
    {
      debug_msg (2, "<= skip parse server key exchange");
      ssl->state++;
      return (0);
    }
  ((void) p);
  ((void) end);

  if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDH_RSA ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDH_ECDSA)
    {
      if ((ret = ssl_get_ecdh_params_from_cert (ssl)) != 0)
        {
          SSL_DEBUG_RET (1, "ssl_get_ecdh_params_from_cert", ret);
          return (ret);
        }

      debug_msg (2, "<= skip parse server key exchange");
      ssl->state++;
      return (0);
    }
  ((void) p);
  ((void) end);

  if ((ret = _ntbtls_read_record (ssl)) != 0)
    {
      debug_ret (1, "read_record", ret);
      return (ret);
    }

  if (ssl->in_msgtype != TLS_MSG_HANDSHAKE)
    {
      debug_msg (1, "bad server key exchange message");
      return gpg_error (GPG_ERR_UNEXPECTED_MSG);
    }

  /*
   * ServerKeyExchange may be skipped with PSK and RSA-PSK when the server
   * doesn't use a psk_identity_hint
   */
  if (ssl->in_msg[0] != TLS_HS_SERVER_KEY_EXCHANGE)
    {
      if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
          ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK)
        {
          ssl->record_read = 1;
          goto exit;
        }

      debug_msg (1, "bad server key exchange message");
      return gpg_error (GPG_ERR_UNEXPECTED_MSG);
    }

  p = ssl->in_msg + 4;
  end = ssl->in_msg + ssl->in_hslen;
  debug_buf (3, "server key exchange", p, ssl->in_hslen - 4);

  if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK)
    {
      if (ssl_parse_server_psk_hint (ssl, &p, end) != 0)
        {
          debug_msg (1, "bad server key exchange message");
          return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
        }
    }                           /* FALLTROUGH */


  if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK)
    ;                           /* nothing more to do */
  else if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_RSA ||
           ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK)
    {
      if (ssl_parse_server_dh_params (ssl, &p, end) != 0)
        {
          debug_msg (1, "bad server key exchange message");
          return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
        }
    }
  else if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA)
    {
      if (ssl_parse_server_ecdh_params (ssl, &p, end) != 0)
        {
          debug_msg (1, "bad server key exchange message");
          return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
        }
    }
  else
    {
      debug_bug ();
      return gpg_error (GPG_ERR_INTERNAL);
    }


  if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_RSA ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_RSA ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA)
    {
      params_len = p - (ssl->in_msg + 4);

      /*
       * Handle the digitally-signed structure
       */
      if (ssl->minor_ver == SSL_MINOR_VERSION_3)
        {
          if (ssl_parse_signature_algorithm (ssl, &p, end,
                                             &md_alg, &pk_alg) != 0)
            {
              debug_msg (1, "bad server key exchange message");
              return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
            }

          if (pk_alg != ssl_get_ciphersuite_sig_pk_alg (ciphersuite_info))
            {
              debug_msg (1, "bad server key exchange message");
              return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
            }
        }
      else
        {
          debug_bug ();
          return gpg_error (GPG_ERR_INTERNAL);
        }

      /*
       * Read signature
       */
      sig_len = (p[0] << 8) | p[1];
      p += 2;

      if (end != p + sig_len)
        {
          debug_msg (1, "bad server key exchange message");
          return gpg_error (GPG_ERR_BAD_HS_SERVER_KEX);
        }

      debug_buf (3, "signature", p, sig_len);

      /*
       * Compute the hash that has been signed
       */
      if (md_alg != POLARSSL_MD_NONE)
        {
          md_context_t ctx;

          md_init (&ctx);

          /* Info from md_alg will be used instead */
          hashlen = 0;

          /*
           * digitally-signed struct {
           *     opaque client_random[32];
           *     opaque server_random[32];
           *     ServerDHParams params;
           * };
           */
          if ((ret = md_init_ctx (&ctx, md_info_from_type (md_alg))) != 0)
            {
              SSL_DEBUG_RET (1, "md_init_ctx", ret);
              return (ret);
            }

          md_starts (&ctx);
          md_update (&ctx, ssl->handshake->randbytes, 64);
          md_update (&ctx, ssl->in_msg + 4, params_len);
          md_finish (&ctx, hash);
          md_free (&ctx);
        }
      else
        {
          debug_bug ();
          return gpg_error (GPG_ERR_INTERNAL);
        }

      debug_buf (3, "parameters hash", hash, hashlen != 0 ? hashlen :
                     (unsigned int) (md_info_from_type (md_alg))->size);

      /*
       * Verify signature
       */
      if (!pk_can_do (&ssl->session_negotiate->peer_cert->pk, pk_alg))
        {
          debug_msg (1, "bad server key exchange message");
          return gpg_error (GPG_ERR_WRONG_PUBKEY_ALGO);
        }

      if ((ret = pk_verify (&ssl->session_negotiate->peer_cert->pk,
                            md_alg, hash, hashlen, p, sig_len)) != 0)
        {
          SSL_DEBUG_RET (1, "pk_verify", ret);
          return (ret);
        }
    }

exit:
  ssl->state++;

  debug_msg (2, "<= parse server key exchange");

  return (0);
}

static int
parse_certificate_request (ntbtls_t ssl)
{
  int ret;
  unsigned char *buf, *p;
  size_t n = 0, m = 0;
  size_t cert_type_len = 0, dn_len = 0;
  const ssl_ciphersuite_t *ciphersuite_info =
    ssl->transform_negotiate->ciphersuite_info;

  debug_msg (2, "=> parse certificate request");

  if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK)
    {
      debug_msg (2, "<= skip parse certificate request");
      ssl->state++;
      return (0);
    }

  /*
   *     0  .   0   handshake type
   *     1  .   3   handshake length
   *     4  .   4   cert type count
   *     5  .. m-1  cert types
   *     m  .. m+1  sig alg length (TLS 1.2 only)
   *    m+1 .. n-1  SignatureAndHashAlgorithms (TLS 1.2 only)
   *     n  .. n+1  length of all DNs
   *    n+2 .. n+3  length of DN 1
   *    n+4 .. ...  Distinguished Name #1
   *    ... .. ...  length of DN 2, etc.
   */
  if (ssl->record_read == 0)
    {
      if ((ret = _ntbtls_read_record (ssl)) != 0)
        {
          debug_ret (1, "read_record", ret);
          return (ret);
        }

      if (ssl->in_msgtype != TLS_MSG_HANDSHAKE)
        {
          debug_msg (1, "bad certificate request message");
          return gpg_error (GPG_ERR_UNEXPECTED_MSG);
        }

      ssl->record_read = 1;
    }

  ssl->client_auth = 0;
  ssl->state++;

  if (ssl->in_msg[0] == TLS_HS_CERTIFICATE_REQUEST)
    ssl->client_auth++;

  debug_msg (3, "got %s certificate request",
             ssl->client_auth ? "a" : "no");

  if (ssl->client_auth == 0)
    goto exit;

  ssl->record_read = 0;

  // TODO: handshake_failure alert for an anonymous server to request
  // client authentication

  buf = ssl->in_msg;

  // Retrieve cert types
  //
  cert_type_len = buf[4];
  n = cert_type_len;

  if (ssl->in_hslen < 6 + n)
    {
      debug_msg (1, "bad certificate request message");
      return gpg_error (GPG_ERR_BAD_HS_CERT_REQ);
    }

  p = buf + 5;
  while (cert_type_len > 0)
    {
      if (*p == SSL_CERT_TYPE_RSA_SIGN &&
          pk_can_do (ssl_own_key (ssl), POLARSSL_PK_RSA))
        {
          ssl->handshake->cert_type = SSL_CERT_TYPE_RSA_SIGN;
          break;
        }
      else if (*p == SSL_CERT_TYPE_ECDSA_SIGN &&
            pk_can_do (ssl_own_key (ssl), POLARSSL_PK_ECDSA))
        {
          ssl->handshake->cert_type = SSL_CERT_TYPE_ECDSA_SIGN;
          break;
        }
      else
        {
          /* Unsupported cert type, ignore */
        }

      cert_type_len--;
      p++;
    }

  if (ssl->minor_ver == SSL_MINOR_VERSION_3)
    {
      /* Ignored, see comments about hash in write_certificate_verify */
      // TODO: should check the signature part against our pk_key though
      size_t sig_alg_len = ((buf[5 + n] << 8) | (buf[6 + n]));

      p = buf + 7 + n;
      m += 2;
      n += sig_alg_len;

      if (ssl->in_hslen < 6 + n)
        {
          debug_msg (1, "bad certificate request message");
          return gpg_error (GPG_ERR_BAD_HS_CERT_REQ);
        }
    }

  /* Ignore certificate_authorities, we only have one cert anyway */
  // TODO: should not send cert if no CA matches
  dn_len = ((buf[5 + m + n] << 8) | (buf[6 + m + n]));

  n += dn_len;
  if (ssl->in_hslen != 7 + m + n)
    {
      debug_msg (1, "bad certificate request message");
      return gpg_error (GPG_ERR_BAD_HS_CERT_REQ);
    }

 exit:
  debug_msg (2, "<= parse certificate request");

  return (0);
}


static int
parse_server_hello_done (ntbtls_t ssl)
{
  int ret;

  debug_msg (2, "=> parse server hello done");

  if (ssl->record_read == 0)
    {
      if ((ret = _ntbtls_read_record (ssl)) != 0)
        {
          debug_ret (1, "ssl_read_record", ret);
          return (ret);
        }

      if (ssl->in_msgtype != TLS_MSG_HANDSHAKE)
        {
          debug_msg (1, "bad server hello done message");
          return gpg_error (GPG_ERR_UNEXPECTED_MSG);
        }
    }
  ssl->record_read = 0;

  if (ssl->in_hslen != 4 || ssl->in_msg[0] != TLS_HS_SERVER_HELLO_DONE)
    {
      debug_msg (1, "bad server hello done message");
      return gpg_error (GPG_ERR_BAD_HS_SERVER_HELLO_DONE);
    }

  ssl->state++;

  debug_msg (2, "<= parse server hello done");

  return (0);
}

static int
write_client_key_exchange (ntbtls_t ssl)
{
  int ret;
  size_t i, n;
  const ssl_ciphersuite_t *ciphersuite_info =
    ssl->transform_negotiate->ciphersuite_info;

  debug_msg (2, "=> write client key exchange");

  if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_RSA)
    {
      /*
       * DHM key exchange -- send G^X mod P
       */
      n = ssl->handshake->dhm_ctx.len;

      ssl->out_msg[4] = (unsigned char) (n >> 8);
      ssl->out_msg[5] = (unsigned char) (n);
      i = 6;

      ret = dhm_make_public (&ssl->handshake->dhm_ctx,
                             (int) mpi_size (&ssl->handshake->dhm_ctx.P),
                             &ssl->out_msg[i], n, ssl->f_rng, ssl->p_rng);
      if (ret != 0)
        {
          SSL_DEBUG_RET (1, "dhm_make_public", ret);
          return (ret);
        }

      SSL_DEBUG_MPI (3, "DHM: X ", &ssl->handshake->dhm_ctx.X);
      SSL_DEBUG_MPI (3, "DHM: GX", &ssl->handshake->dhm_ctx.GX);

      ssl->handshake->pmslen = POLARSSL_PREMASTER_SIZE;

      if ((ret = dhm_calc_secret (&ssl->handshake->dhm_ctx,
                                  ssl->handshake->premaster,
                                  &ssl->handshake->pmslen,
                                  ssl->f_rng, ssl->p_rng)) != 0)
        {
          SSL_DEBUG_RET (1, "dhm_calc_secret", ret);
          return (ret);
        }

      SSL_DEBUG_MPI (3, "DHM: K ", &ssl->handshake->dhm_ctx.K);
    }
  else if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA
        || ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDH_RSA
        || ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDH_ECDSA)
    {
      /*
       * ECDH key exchange -- send client public value
       */
      i = 4;

      ret = ecdh_make_public (&ssl->handshake->ecdh_ctx,
                              &n,
                              &ssl->out_msg[i], 1000, ssl->f_rng, ssl->p_rng);
      if (ret != 0)
        {
          SSL_DEBUG_RET (1, "ecdh_make_public", ret);
          return (ret);
        }

      SSL_DEBUG_ECP (3, "ECDH: Q", &ssl->handshake->ecdh_ctx.Q);

      if ((ret = ecdh_calc_secret (&ssl->handshake->ecdh_ctx,
                                   &ssl->handshake->pmslen,
                                   ssl->handshake->premaster,
                                   POLARSSL_MPI_MAX_SIZE,
                                   ssl->f_rng, ssl->p_rng)) != 0)
        {
          SSL_DEBUG_RET (1, "ecdh_calc_secret", ret);
          return (ret);
        }

      SSL_DEBUG_MPI (3, "ECDH: z", &ssl->handshake->ecdh_ctx.z);
    }
  else if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK)
    {
      /*
       * opaque psk_identity<0..2^16-1>;
       */
      if (ssl->psk == NULL || ssl->psk_identity == NULL)
        return gpg_error (GPG_ERR_NO_SECKEY);

      i = 4;
      n = ssl->psk_identity_len;
      ssl->out_msg[i++] = (unsigned char) (n >> 8);
      ssl->out_msg[i++] = (unsigned char) (n);

      memcpy (ssl->out_msg + i, ssl->psk_identity, ssl->psk_identity_len);
      i += ssl->psk_identity_len;

      if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK)
        {
          n = 0;
        }
      else if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK)
        {
          if ((ret = ssl_write_encrypted_pms (ssl, i, &n, 2)) != 0)
            return (ret);
        }
      else if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK)
        {
          /*
           * ClientDiffieHellmanPublic public (DHM send G^X mod P)
           */
          n = ssl->handshake->dhm_ctx.len;
          ssl->out_msg[i++] = (unsigned char) (n >> 8);
          ssl->out_msg[i++] = (unsigned char) (n);

          ret = dhm_make_public (&ssl->handshake->dhm_ctx,
                                 (int) mpi_size (&ssl->handshake->dhm_ctx.P),
                                 &ssl->out_msg[i], n, ssl->f_rng, ssl->p_rng);
          if (ret != 0)
            {
              SSL_DEBUG_RET (1, "dhm_make_public", ret);
              return (ret);
            }
        }
      else if (ciphersuite_info->key_exchange
               == POLARSSL_KEY_EXCHANGE_ECDHE_PSK)
        {
          /*
           * ClientECDiffieHellmanPublic public;
           */
          ret = ecdh_make_public (&ssl->handshake->ecdh_ctx, &n,
                                  &ssl->out_msg[i], SSL_MAX_CONTENT_LEN - i,
                                  ssl->f_rng, ssl->p_rng);
          if (ret != 0)
            {
              SSL_DEBUG_RET (1, "ecdh_make_public", ret);
              return (ret);
            }

          SSL_DEBUG_ECP (3, "ECDH: Q", &ssl->handshake->ecdh_ctx.Q);
        }
      else
        {
          debug_bug ();
          return gpg_error (GPG_ERR_INTERNAL);
        }

      if ((ret = ssl_psk_derive_premaster (ssl,
                                           ciphersuite_info->key_exchange)) !=
          0)
        {
          SSL_DEBUG_RET (1, "ssl_psk_derive_premaster", ret);
          return (ret);
        }
    }
  else if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA)
    {
      i = 4;
      if ((ret = ssl_write_encrypted_pms (ssl, i, &n, 0)) != 0)
        return (ret);
    }
  else
    {
      debug_bug ();
      return gpg_error (GPG_ERR_INTERNAL);
    }

  if ((ret = _ntbtls_derive_keys (ssl)) != 0)
    {
      SSL_DEBUG_RET (1, "ssl_derive_keys", ret);
      return (ret);
    }

  ssl->out_msglen = i + n;
  ssl->out_msgtype = TLS_MSG_HANDSHAKE;
  ssl->out_msg[0] = TLS_HS_CLIENT_KEY_EXCHANGE;

  ssl->state++;

  if ((ret = ssl_write_record (ssl)) != 0)
    {
      SSL_DEBUG_RET (1, "ssl_write_record", ret);
      return (ret);
    }

  debug_msg (2, "<= write client key exchange");

  return (0);
}


static int
write_certificate_verify (ntbtls_t ssl)
{
  int ret = gpg_error (GPG_ERR_NOT_IMPLEMENTED);
  const ssl_ciphersuite_t *ciphersuite_info =
    ssl->transform_negotiate->ciphersuite_info;
  size_t n = 0, offset = 0;
  unsigned char hash[48];
  unsigned char *hash_start = hash;
  md_type_t md_alg = POLARSSL_MD_NONE;
  unsigned int hashlen;

  debug_msg (2, "=> write certificate verify");

  if (ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK ||
      ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK)
    {
      debug_msg (2, "<= skip write certificate verify");
      ssl->state++;
      return (0);
    }

  if (ssl->client_auth == 0 || ssl_own_cert (ssl) == NULL)
    {
      debug_msg (2, "<= skip write certificate verify");
      ssl->state++;
      return (0);
    }

  if (ssl_own_key (ssl) == NULL)
    {
      debug_msg (1, "got no private key");
      return gpg_error (GPG_ERR_NO_SECKEY);
    }

  /*
   * Make an RSA signature of the handshake digests
   */
  ssl->handshake->calc_verify (ssl, hash);

  if (ssl->minor_ver == SSL_MINOR_VERSION_3)
    {
      /*
       * digitally-signed struct {
       *     opaque handshake_messages[handshake_messages_length];
       * };
       *
       * Taking shortcut here. We assume that the server always allows the
       * PRF Hash function and has sent it in the allowed signature
       * algorithms list received in the Certificate Request message.
       *
       * Until we encounter a server that does not, we will take this
       * shortcut.
       *
       * Reason: Otherwise we should have running hashes for SHA512 and SHA224
       *         in order to satisfy 'weird' needs from the server side.
       */
      if (ssl->transform_negotiate->ciphersuite_info->mac ==
          POLARSSL_MD_SHA384)
        {
          md_alg = POLARSSL_MD_SHA384;
          ssl->out_msg[4] = SSL_HASH_SHA384;
        }
      else
        {
          md_alg = POLARSSL_MD_SHA256;
          ssl->out_msg[4] = SSL_HASH_SHA256;
        }
      ssl->out_msg[5] = 0; //FIXME: ssl_sig_from_pk (ssl_own_key (ssl));

      /* Info from md_alg will be used instead */
      hashlen = 0;
      offset = 2;
    }
  else
    {
      debug_bug ();
      return gpg_error (GPG_ERR_INTERNAL);
    }

  if ((ret = pk_sign (ssl_own_key (ssl), md_alg, hash_start, hashlen,
                      ssl->out_msg + 6 + offset, &n,
                      ssl->f_rng, ssl->p_rng)) != 0)
    {
      SSL_DEBUG_RET (1, "pk_sign", ret);
      return (ret);
    }

  ssl->out_msg[4 + offset] = (unsigned char) (n >> 8);
  ssl->out_msg[5 + offset] = (unsigned char) (n);

  ssl->out_msglen = 6 + n + offset;
  ssl->out_msgtype = TLS_MSG_HANDSHAKE;
  ssl->out_msg[0] = TLS_HS_CERTIFICATE_VERIFY;

  ssl->state++;

  if ((ret = ssl_write_record (ssl)) != 0)
    {
      SSL_DEBUG_RET (1, "ssl_write_record", ret);
      return (ret);
    }

  debug_msg (2, "<= write certificate verify");

  return (ret);
}


static int
parse_new_session_ticket (ntbtls_t ssl)
{
  int ret;
  uint32_t lifetime;
  size_t ticket_len;
  unsigned char *ticket;

  debug_msg (2, "=> parse new session ticket");

  if ((ret = _ntbtls_read_record (ssl)) != 0)
    {
      debug_ret (1, "read_record", ret);
      return (ret);
    }

  if (ssl->in_msgtype != TLS_MSG_HANDSHAKE)
    {
      debug_msg (1, "bad new session ticket message");
      return gpg_error (GPG_ERR_UNEXPECTED_MSG);
    }

  /*
   * struct {
   *     uint32 ticket_lifetime_hint;
   *     opaque ticket<0..2^16-1>;
   * } NewSessionTicket;
   *
   * 0  .  0   handshake message type
   * 1  .  3   handshake message length
   * 4  .  7   ticket_lifetime_hint
   * 8  .  9   ticket_len (n)
   * 10 .  9+n ticket content
   */
  if (ssl->in_msg[0] != TLS_HS_NEW_SESSION_TICKET || ssl->in_hslen < 10)
    {
      debug_msg (1, "bad new session ticket message");
      return gpg_error (GPG_ERR_BAD_TICKET);
    }

  lifetime = (ssl->in_msg[4] << 24) | (ssl->in_msg[5] << 16) |
    (ssl->in_msg[6] << 8) | (ssl->in_msg[7]);

  ticket_len = (ssl->in_msg[8] << 8) | (ssl->in_msg[9]);

  if (ticket_len + 10 != ssl->in_hslen)
    {
      debug_msg (1, "bad new session ticket message");
      return gpg_error (GPG_ERR_BAD_TICKET);
    }

  debug_msg (3, "ticket length: %d", ticket_len);

  /* We're not waiting for a NewSessionTicket message any more */
  ssl->handshake->new_session_ticket = 0;

  /*
   * Zero-length ticket means the server changed his mind and doesn't want
   * to send a ticket after all, so just forget it
   */
  if (ticket_len == 0)
    return (0);

  wipememory (ssl->session_negotiate->ticket,
              ssl->session_negotiate->ticket_len);
  polarssl_free (ssl->session_negotiate->ticket);
  ssl->session_negotiate->ticket = NULL;
  ssl->session_negotiate->ticket_len = 0;

  ticket = malloc (ticket_len);
  if (!ticket)
    {
      err = gpg_error_from_syserror ();
      debug_msg (1, "ticket malloc failed");
      return err;
    }

  memcpy (ticket, ssl->in_msg + 10, ticket_len);

  ssl->session_negotiate->ticket = ticket;
  ssl->session_negotiate->ticket_len = ticket_len;
  ssl->session_negotiate->ticket_lifetime = lifetime;

  /*
   * RFC 5077 section 3.4:
   * "If the client receives a session ticket from the server, then it
   * discards any Session ID that was sent in the ServerHello."
   */
  debug_msg (3, "ticket in use, discarding session id");
  ssl->session_negotiate->length = 0;

  debug_msg (2, "<= parse new session ticket");

  return (0);
}


/*
 * SSL handshake -- client side -- single step
 */
gpg_error_t
_ntbtls_handshake_client_step (ntbtls_t tls)
{
  gpg_error_t err;

  if (tls->state == TLS_HANDSHAKE_OVER)
    return gpg_error (GPG_ERR_INV_STATE)

  debug_msg (2, "client state: %d", tls->state);

  err = _ntbtls_flush_output (tls);
  if (err)
    return err;

  switch (tls->state)
    {
    case TLS_HELLO_REQUEST:
      tls->state = TLS_CLIENT_HELLO;
      break;

      /*
       *  ==>   ClientHello
       */
    case TLS_CLIENT_HELLO:
      err = write_client_hello (tls);
      break;

      /*
       *  <==   ServerHello
       *        Certificate
       *      ( ServerKeyExchange  )
       *      ( CertificateRequest )
       *        ServerHelloDone
       */
    case TLS_SERVER_HELLO:
      err = parse_server_hello (tls);
      break;

    case TLS_SERVER_CERTIFICATE:
      err = _ntbtls_parse_certificate (tls);
      break;

    case TLS_SERVER_KEY_EXCHANGE:
      err = parse_server_key_exchange (tls);
      break;

    case TLS_CERTIFICATE_REQUEST:
      err = parse_certificate_request (tls);
      break;

    case TLS_SERVER_HELLO_DONE:
      err = parse_server_hello_done (tls);
      break;

      /*
       *  ==> ( Certificate/Alert  )
       *        ClientKeyExchange
       *      ( CertificateVerify  )
       *        ChangeCipherSpec
       *        Finished
       */
    case TLS_CLIENT_CERTIFICATE:
      err = _ntbtls_write_certificate (tls);
      break;

    case TLS_CLIENT_KEY_EXCHANGE:
      err = write_client_key_exchange (tls);
      break;

    case TLS_CERTIFICATE_VERIFY:
      err = write_certificate_verify (tls);
      break;

    case TLS_CLIENT_CHANGE_CIPHER_SPEC:
      err = _ntbtls_write_change_cipher_spec (tls);
      break;

    case TLS_CLIENT_FINISHED:
      err = _ntbtls_write_finished (tls);
      break;

      /*
       *  <==   ( NewSessionTicket )
       *        ChangeCipherSpec
       *        Finished
       */
    case TLS_SERVER_CHANGE_CIPHER_SPEC:
      if (tls->handshake->new_session_ticket)
        err = parse_new_session_ticket (tls);
      else
        err = _ntbtls_parse_change_cipher_spec (tls);
      break;

    case TLS_SERVER_FINISHED:
      err = _ntbtls_parse_finished (tls);
      break;

    case TLS_FLUSH_BUFFERS:
      TLS_DEBUG_MSG (2, "handshake: done");
      tls->state = TLS_HANDSHAKE_WRAPUP;
      break;

    case TLS_HANDSHAKE_WRAPUP:
      _ntbtls_handshake_wrapup (tls);
      break;

    default:
      TLS_DEBUG_MSG (1, "invalid state %d", tls->state);
      err = gpg_error (GPG_ERR_INV_STATE);
      break;
    }

  return err;
}