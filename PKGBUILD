_pkgbase=ryzen_smu_hwmon
pkgname=ryzen_smu_hwmon-dkms
pkgver=0.1
pkgrel=1
pkgdesc="Hardware monitoring driver using the Ryzen SMU exported API"
arch=('x86_64')
license=('GPL2')

depends=('dkms' 'ryzen_smu-dkms-git')

provides=('ryzen_smu_hwmon-dkms')
conflicts=('ryzen_smu_hwmon-dkms')

source=(
  "dkms.conf"
  "Makefile"
  "ryzen_smu_hwmon.c"
  "README.md"
)

sha256sums=('SKIP' 'SKIP' 'SKIP' 'SKIP')

package() {
  install -d "$pkgdir/usr/src/${_pkgbase}-${pkgver}"

  cp dkms.conf Makefile ryzen_smu_hwmon.c README.md \
     "$pkgdir/usr/src/${_pkgbase}-${pkgver}/"

  sed -e "s/@PKGVER@/${pkgver}/" \
      -i "$pkgdir/usr/src/${_pkgbase}-${pkgver}/dkms.conf"
}
