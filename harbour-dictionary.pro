TEMPLATE = subdirs

quazip_lib.file = quazip/quazip/dictionary-quazip.pro
quazip_lib.target = quazip-lib

app_src.subdir = src
app_src.target = app-src
app_src.depends = quazip-lib

SUBDIRS = quazip_lib app_src

OTHER_FILES += rpm/harbour-dictionary.changes.in \
    rpm/harbour-dictionary.spec \
    rpm/harbour-dictionary.yaml \
    translations/*.ts \
    harbour-dictionary.desktop \
    qml/components/*.qml \
    qml/sf-docked-tab-bar/*.qml \
    qml/pages/*.qml \
    qml/*.qml
    db/heinzelliste.db

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256

# to disable building translations every time, comment out the
# following CONFIG line
CONFIG += sailfishapp_i18n

# German translation is enabled as an example. If you aren't
# planning to localize your app, remember to comment out the
# following TRANSLATIONS line. And also do not forget to
# modify the localized app name in the the .desktop file.
TRANSLATIONS += translations/harbour-dictionary-de.ts \
     translations/harbour-dictionary-fr.ts \
     translations/harbour-dictionary-it.ts \
     translations/harbour-dictionary-nl.ts \
     translations/harbour-dictionary-sv.ts \
     translations/harbour-dictionary-es.ts \
     translations/harbour-dictionary-pl.ts \
     translations/harbour-dictionary-hu.ts \
     translations/harbour-dictionary-zh_CN.ts

database.files = db
database.path = /usr/share/$${TARGET}

gui.files = qml
gui.path = /usr/share/$${TARGET}

images.files = images
images.path = /usr/share/$${TARGET}

ICONPATH = /usr/share/icons/hicolor

86.png.path = $${ICONPATH}/86x86/apps/
86.png.files += icons/86x86/harbour-dictionary.png

108.png.path = $${ICONPATH}/108x108/apps/
108.png.files += icons/108x108/harbour-dictionary.png

128.png.path = $${ICONPATH}/128x128/apps/
128.png.files += icons/128x128/harbour-dictionary.png

172.png.path = $${ICONPATH}/172x172/apps/
172.png.files += icons/172x172/harbour-dictionary.png

256.png.path = $${ICONPATH}/256x256/apps/
256.png.files += icons/256x256/harbour-dictionary.png

dictionary.desktop.path = /usr/share/applications/
dictionary.desktop.files = harbour-dictionary.desktop

INSTALLS += 86.png 108.png 128.png 172.png 256.png \
            dictionary.desktop database gui images

DISTFILES += \
    icons/86x86/harbour-dictionary.png \
    icons/108x108/harbour-dictionary.png \
    icons/128x128/harbour-dictionary.png \
    icons/172x172/harbour-dictionary.png \
    icons/256x256/harbour-dictionary.png \
    images/*.png \
    qml/*.qml \
    pml/components/*.qml \
    qml/sf-docked-tab-bar/*.qml \
    pml/pages/*.qml \
    LICENSE \
    README.md \
