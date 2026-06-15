#!/bin/bash
# One-time (or recovery) setup of App Store distribution signing, fully via
# the App Store Connect API key — no Xcode GUI, no cloud signing.
#
# Why this exists: the App Manager API key canNOT do "cloud signing" (the
# error "Cloud signing permission error" during xcodebuild -exportArchive).
# So we create the distribution certificate + App Store provisioning profile
# ourselves through the API, import the cert into the login keychain, and
# install the profile. deploy-testflight.sh then exports with manual signing.
#
# Run this if the distribution cert/profile is missing or expired
# (Apple distribution certs + profiles last ~1 year).
set -e

KEY_ID="${ASC_KEY_ID:-799J6X3X8U}"
ISSUER_ID="${ASC_ISSUER_ID:-44a51cfb-d669-4bdb-97a8-688a583732b9}"
KEY_PATH="${ASC_KEY_PATH:-$HOME/.appstoreconnect/private_keys/AuthKey_$KEY_ID.p8}"
BUNDLE_ID="com.terryx.retroread"
PROFILE_NAME="RetroRead AppStore"
P12_PASS="retroread"
WORK=$(mktemp -d)

echo "=== Generating CSR ==="
openssl req -new -newkey rsa:2048 -nodes \
    -keyout "$WORK/dist_key.pem" -out "$WORK/dist.csr" \
    -subj "/CN=RetroRead Distribution/O=Taoran Xue/C=US" 2>/dev/null

echo "=== Creating distribution cert + App Store profile via API ==="
python3 - "$KEY_ID" "$ISSUER_ID" "$KEY_PATH" "$BUNDLE_ID" "$PROFILE_NAME" "$WORK" <<'PY'
import jwt, time, json, urllib.request, urllib.error, base64, os, sys
KEY_ID, ISSUER, KEY_PATH, BUNDLE_ID, PROFILE_NAME, WORK = sys.argv[1:7]
key=open(KEY_PATH).read()
def tok():
    now=int(time.time())
    return jwt.encode({"iss":ISSUER,"iat":now,"exp":now+300,"aud":"appstoreconnect-v1"},key,algorithm="ES256",headers={"kid":KEY_ID})
def api(path,method="GET",body=None):
    req=urllib.request.Request("https://api.appstoreconnect.apple.com"+path,
        data=json.dumps(body).encode() if body else None,
        headers={"Authorization":f"Bearer {tok()}","Content-Type":"application/json"},method=method)
    try: return json.load(urllib.request.urlopen(req))
    except urllib.error.HTTPError as e:
        print("API ERROR",e.code,e.read().decode()[:500]); sys.exit(1)

csr=open(os.path.join(WORK,"dist.csr")).read()
cert=api("/v1/certificates","POST",{"data":{"type":"certificates","attributes":{"certificateType":"DISTRIBUTION","csrContent":csr}}})
cid=cert["data"]["id"]
open(os.path.join(WORK,"dist_cert.b64"),"w").write(cert["data"]["attributes"]["certificateContent"])
print("cert id:",cid)

bids=api("/v1/bundleIds?limit=200")
brid=next(b["id"] for b in bids["data"] if b["attributes"]["identifier"]==BUNDLE_ID)

# delete any existing profile with same name to avoid duplicate-name errors
for p in api("/v1/profiles?limit=200").get("data",[]):
    if p["attributes"]["name"]==PROFILE_NAME:
        api(f"/v1/profiles/{p['id']}","DELETE")

prof=api("/v1/profiles","POST",{"data":{"type":"profiles",
    "attributes":{"name":PROFILE_NAME,"profileType":"IOS_APP_STORE"},
    "relationships":{"bundleId":{"data":{"type":"bundleIds","id":brid}},
        "certificates":{"data":[{"type":"certificates","id":cid}]}}}})
a=prof["data"]["attributes"]
dest=os.path.expanduser("~/Library/MobileDevice/Provisioning Profiles")
os.makedirs(dest,exist_ok=True)
open(os.path.join(dest,a["uuid"]+".mobileprovision"),"wb").write(base64.b64decode(a["profileContent"]))
print("profile installed:",a["uuid"])
PY

echo "=== Importing cert into login keychain ==="
python3 -c "import base64;open('$WORK/dist_cert.der','wb').write(base64.b64decode(open('$WORK/dist_cert.b64').read()))"
openssl x509 -inform DER -in "$WORK/dist_cert.der" -out "$WORK/dist_cert.pem"
openssl pkcs12 -export -legacy -inkey "$WORK/dist_key.pem" -in "$WORK/dist_cert.pem" \
    -out "$WORK/dist.p12" -passout pass:$P12_PASS
security import "$WORK/dist.p12" -k ~/Library/Keychains/login.keychain-db \
    -P "$P12_PASS" -T /usr/bin/codesign -T /usr/bin/security

rm -rf "$WORK"
echo "=== Done. Distribution identity: ==="
security find-identity -v -p codesigning | grep -i distribution
echo "Now run ./deploy-testflight.sh"
