from astropy_helpers.git_helpers import get_git_devstr

__all__ = [
    "__title__", "__summary__", "__uri__", "__version__", "__author__",
    "__email__", "__license__", "__copyright__",
]

__title__ = "nihts_xcam"
__summary__ = "Basic control software for the Xenics XEVA near-infrared slit-viewing camera on Lowell DCT NIHTS."
__uri__ = "https://github.com/henryroe/NIHTS-xcam"

# VERSION should be PEP386 compatible (http://www.python.org/dev/peps/pep-0386)
__version__ = "0.2.0dev"

# Indicates if this version is a release version
RELEASE = 'dev' not in __version__

if not RELEASE:
    __version__ += get_git_devstr(False)

__author__ = "Henry Roe"
__email__ = "hroe@hroe.me"

__license__ = "MIT License"
__copyright__ = "2017 %s" % __author__
