# Makefile to (re)create the EME test fixtures in this directory from scratch.
#
# Tools required on PATH: packager (Shaka Packager)
#
# Targets:
#   make                                -> build all final .mp4 fixtures
#   make video_512x288_h264-360k_enc_dashinit.mp4   (etc.)  -> single fixture
#   make clean                          -> remove intermediates (PSSH bins,
#                                          /tmp/*.mp4 splice parts) but NOT final
#                                          fixtures
#   make dist-clean                     -> remove everything including final
#                                          fixtures
#
# Pipeline:
#   The clear DASH-init files (video_..._clear_dashinit.mp4 and
#   audio_..._dashinit.mp4) are the inputs. Shaka Packager encrypts
#   the clear input with raw-key cbcs and embeds new PSSH atoms.
#
# The recipes are not byte-for-byte reproductions of the existing shipped
# files — they are a regeneratable skeleton matching the content-metadata.js
# contract (KIDs, keys, asset IDs, PSSH systemIDs).

# ---- Content metadata (from content-metadata.js) ----------------------------
# Asset IDs
ASSET_BASIC                := mp4-basic
ASSET_MULTIKEY             := mp4-multikey
ASSET_MULTIKEY_SEQ         := mp4-multikey-sequential

# mp4-basic / mp4-clear-encrypted / mp4-encrypted-clear / mp4-av-multikey (video)
KID_BASIC                  := ad13f9ea2be698b875f504a8e3ccea64
KEY_BASIC                  := be7df8a3667a6a8fd564d0ed81339a95
IV_BASIC                   := 3d6310382283138188b6077accc45fd7

# mp4-av-multikey audio
KID_AV_AUDIO               := 558ee541b90ab2f3950d00ade3760d45
KEY_AV_AUDIO               := 91039263016da635770d57db92f98bd0
IV_AV_AUDIO                := 09b292f5a4bfd3a46611d787f0ddb9d3

# mp4-multikey (two keys, two sessions)
KID_MULTIKEY_1             := 13a75306d118917b47a6c1836442516f
KEY_MULTIKEY_1             := 8aaad8c4dbdeaccdad2676a1ed38952e
IV_MULTIKEY_1              := 4a112f13c154a48d885a8c77cce1d453
KID_MULTIKEY_2             := ee73564ec8a890f078ef6871fa4be18b
KEY_MULTIKEY_2             := e44fe1457c5ebcd83eaddcd62caf5518
IV_MULTIKEY_2              := 49361efe6431298d3defe25540171fde

# mp4-multikey-sequential (two keys, sequential rotation in same track)
KID_MULTIKEY_SEQ_1         := 8a0d85452105d415358fea8f68e6c191
KEY_MULTIKEY_SEQ_1         := 766fabc1683ff8ef4e760024c5238f10
IV_MULTIKEY_SEQ_1          := 83a56501fc6134e830a9c483ccd5afb6
KID_MULTIKEY_SEQ_2         := fbb4b7f34abd3187344bcec45f966888
KEY_MULTIKEY_SEQ_2         := 2652c31df792d17b08a6fad37cb62560
IV_MULTIKEY_SEQ_2          := 3cfe12729550a5973b370cecbe9702a1

# Common video input (the clear source for every video fixture).
VIDEO_CLEAR_IN := video_512x288_h264-360k_clear_dashinit.mp4
AUDIO_CLEAR_IN := audio_aac-lc_128k_dashinit.mp4

# ---- Final fixtures ---------------------------------------------------------
# Encryption uses Shaka Packager with --enable_raw_key_encryption.

# mp4-basic: encrypted video
video_512x288_h264-360k_enc_dashinit.mp4: $(VIDEO_CLEAR_IN)
	packager \
	  input=$<,stream=video,output=$@ \
	  --protection_scheme cbcs \
	  --enable_raw_key_encryption \
	  --keys="label=:key_id=$(KID_BASIC):key=$(KEY_BASIC):iv=$(IV_BASIC)" \
	  --protection_systems="Widevine,PlayReady,FairPlay" \
	  --clear_lead=0 \

# mp4-av-multikey: encrypted audio (separate KID/key)
audio_aac-lc_128k_enc_dashinit.mp4: $(AUDIO_CLEAR_IN)
	packager \
	  input=$<,stream=audio,output=$@ \
	  --protection_scheme cbcs \
	  --enable_raw_key_encryption \
	  --clear_lead=0 \
	  --keys="label=:key_id=$(KID_AV_AUDIO):key=$(KEY_AV_AUDIO):iv=$(IV_AV_AUDIO)" \
	  --protection_systems="Widevine,PlayReady,FairPlay" \

# mp4-multikey: two-key files (encrypted with KEY_MULTIKEY_1 only for now;
# both KIDs are advertised in the PSSH boxes).
video_512x288_h264-360k_enc_2keys_2sess.mp4: $(VIDEO_CLEAR_IN)
	packager \
	  input=$<,stream=video,output=$@ \
	  --protection_scheme cbcs \
	  --enable_raw_key_encryption \
	  --clear_lead=0 \
	  --keys="label=:key_id=$(KID_MULTIKEY_1):key=$(KEY_MULTIKEY_1):iv=$(IV_MULTIKEY_1),label=0:key_id=$(KID_MULTIKEY_2):key=$(KEY_MULTIKEY_2)" \
	  --protection_systems="Widevine,PlayReady,FairPlay" \

	@echo "WARNING: encrypted with KEY_MULTIKEY_1 only; KEY_MULTIKEY_2 advertised in PSSH but not used for encryption."

audio_aac-lc_128k_2keys_2sess.mp4: $(AUDIO_CLEAR_IN)
	packager \
	  input=$<,stream=audio,output=$@ \
	  --protection_scheme cbcs \
	  --enable_raw_key_encryption \
	  --clear_lead=0 \
	  --keys="label=:key_id=$(KID_MULTIKEY_1):key=$(KEY_MULTIKEY_1)" \
	  --protection_systems="Widevine,PlayReady,FairPlay" \

	@echo "WARNING: encrypted with KEY_MULTIKEY_1 only; KEY_MULTIKEY_2 advertised in PSSH but not used for encryption."

# mp4-multikey-sequential: three independent fragmented MP4s concatenated.
# The original fixture is encrypted with KEY_MULTIKEY_SEQ_1, then
# KEY_MULTIKEY_SEQ_2, then KEY_MULTIKEY_SEQ_1 again — each segment is a
# complete ftyp+moov+moof+mdat sequence, so we just `cat` them.
video_512x288_h264-360k_multikey_dashinit.mp4: $(VIDEO_CLEAR_IN)
	packager \
	  input=$<,stream=video,output=/tmp/multikey-sequential-key1.mp4 \
	  --protection_scheme cbcs \
	  --enable_raw_key_encryption \
	  --clear_lead=0 \
	  --keys="label=:key_id=$(KID_MULTIKEY_SEQ_1):key=$(KEY_MULTIKEY_SEQ_1):iv=$(IV_MULTIKEY_SEQ_1)" \
	  --protection_systems="Widevine,PlayReady,FairPlay" \

	packager \
	  input=$<,stream=video,output=/tmp/multikey-sequential-key2.mp4 \
	  --protection_scheme cbcs \
	  --enable_raw_key_encryption \
	  --clear_lead=0 \
	  --keys="label=:key_id=$(KID_MULTIKEY_SEQ_2):key=$(KEY_MULTIKEY_SEQ_2):iv=$(IV_MULTIKEY_SEQ_2)" \
	  --protection_systems="Widevine,PlayReady,FairPlay" \

	cat /tmp/multikey-sequential-key1.mp4 /tmp/multikey-sequential-key2.mp4 /tmp/multikey-sequential-key1.mp4 > $@
	rm -f /tmp/multikey-sequential-key1.mp4 /tmp/multikey-sequential-key2.mp4

# ---- Clear/encrypted transition fixtures -----------------------------------
# Same approach as multikey_dashinit: concatenate two complete
# ftyp+moov+moof+mdat files. The transition is therefore a re-initialization
# (the result has two moov atoms), which matches the shipped fixtures:
#   * clear_enc -> [clear moov (no tenc)] then [encrypted moov (tenc)]
#   * enc_clear -> [encrypted moov (tenc)] then [clear moov (no tenc)]
#
# Both halves are produced by the same packager from the same clear input, so
# they share identical segmentation and differ only in encryption. The clear
# half is just packager run with no DRM flags. Because Shaka writes fragments
# with default-base-is-moof, sample/aux data offsets are moof-relative, so
# concatenation needs no offset fixups.
#
# Encrypted halves use the BASIC key set (per content-metadata.js: mp4-basic /
# mp4-clear-encrypted / mp4-encrypted-clear share these).

video_512x288_h264-360k_clear_enc_dashinit.mp4: $(VIDEO_CLEAR_IN)
	# Clear half: package with no encryption -> clear moov (no tenc).
	packager \
	  input=$<,stream=video,output=/tmp/clear_enc-clear.mp4 \

	# Encrypted half: cbcs with the BASIC key.
	packager \
	  input=$<,stream=video,output=/tmp/clear_enc-enc.mp4 \
	  --protection_scheme cbcs \
	  --enable_raw_key_encryption \
	  --clear_lead=0 \
	  --keys="label=:key_id=$(KID_BASIC):key=$(KEY_BASIC):iv=$(IV_BASIC)" \
	  --protection_systems="Widevine,PlayReady,FairPlay" \

	cat /tmp/clear_enc-clear.mp4 /tmp/clear_enc-enc.mp4 > $@
	rm -f /tmp/clear_enc-clear.mp4 /tmp/clear_enc-enc.mp4

video_512x288_h264-360k_enc_clear_dashinit.mp4: $(VIDEO_CLEAR_IN)
	# Encrypted half first: cbcs with the BASIC key.
	packager \
	  input=$<,stream=video,output=/tmp/enc_clear-enc.mp4 \
	  --protection_scheme cbcs \
	  --enable_raw_key_encryption \
	  --clear_lead=0 \
	  --keys="label=:key_id=$(KID_BASIC):key=$(KEY_BASIC):iv=$(IV_BASIC)" \
	  --protection_systems="Widevine,PlayReady,FairPlay" \

	# Clear half second: package with no encryption -> clear moov (no tenc).
	packager \
	  input=$<,stream=video,output=/tmp/enc_clear-clear.mp4 \

	cat /tmp/enc_clear-enc.mp4 /tmp/enc_clear-clear.mp4 > $@
	rm -f /tmp/enc_clear-enc.mp4 /tmp/enc_clear-clear.mp4

# ---- Aggregate targets -----------------------------------------------------
FIXTURES := \
	audio_aac-lc_128k_2keys_2sess.mp4 \
	audio_aac-lc_128k_enc_dashinit.mp4 \
	video_512x288_h264-360k_clear_enc_dashinit.mp4 \
	video_512x288_h264-360k_enc_2keys_2sess.mp4 \
	video_512x288_h264-360k_enc_clear_dashinit.mp4 \
	video_512x288_h264-360k_enc_dashinit.mp4 \
	video_512x288_h264-360k_multikey_dashinit.mp4

# Intermediates produced into /tmp during splicing.
INTERMEDIATES := \
	/tmp/clear_enc-clear.mp4 \
	/tmp/clear_enc-enc.mp4 \
	/tmp/enc_clear-clear.mp4 \
	/tmp/enc_clear-enc.mp4 \
	/tmp/multikey-sequential-key1.mp4 \
	/tmp/multikey-sequential-key2.mp4

.PHONY: all clean dist-clean
all: $(FIXTURES)

# clean removes only intermediates, not final fixtures (per header contract).
clean:
	rm -f $(INTERMEDIATES)

# dist-clean removes intermediates AND final fixtures.
dist-clean: clean
	rm -f $(FIXTURES)
