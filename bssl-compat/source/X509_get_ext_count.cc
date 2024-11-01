#include <openssl/x509.h>
#include <ossl.h>


extern "C" int X509_get_ext_count(const X509 *x) {
  return ossl.ossl_X509_get_ext_count(x);
}
