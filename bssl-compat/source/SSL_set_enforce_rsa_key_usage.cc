#include <openssl/ssl.h>
#include <ossl.h>


extern "C" void SSL_set_enforce_rsa_key_usage(SSL *ssl, int enabled) {
  // Provide the function, but do nothing to actually implement it.
  // I don't know how to implement this in OpenSSL - this seem to be a
  // BoringSSL specific feature.
  //
  // Envoy should still be able to work in this case, but I cannot
  // convincingly say that without this function the behavior will be
  // the same as with BorgingSSL.
}
