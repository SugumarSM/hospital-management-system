#!/usr/bin/env python3
"""
Hospital Management System - Local Web Server Runner
Runs a lightweight HTTP server serving frontend files and executing the compiled C CGI binary for all /api requests.
"""

import http.server
import socketserver
import os
import subprocess
import urllib.parse
import sys

PORT = 8080
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
FRONTEND_DIR = os.path.join(BASE_DIR, "frontend")
CGI_EXE = os.path.join(BASE_DIR, "build", "hospital_api.exe" if os.name == "nt" else "hospital_api.cgi")
DB_PATH = os.path.join(BASE_DIR, "database", "hospital.db")

class HospitalHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=FRONTEND_DIR, **kwargs)

    def do_OPTIONS(self):
        self.handle_cgi()

    def do_GET(self):
        if self.path.startswith("/api"):
            self.handle_cgi()
        else:
            super().do_GET()

    def do_POST(self):
        if self.path.startswith("/api"):
            self.handle_cgi()
        else:
            self.send_error(404, "Endpoint not found")

    def do_PUT(self):
        if self.path.startswith("/api"):
            self.handle_cgi()
        else:
            self.send_error(404, "Endpoint not found")

    def do_DELETE(self):
        if self.path.startswith("/api"):
            self.handle_cgi()
        else:
            self.send_error(404, "Endpoint not found")

    def handle_cgi(self):
        parsed = urllib.parse.urlparse(self.path)
        path_info = parsed.path
        query_string = parsed.query or ""

        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length) if content_length > 0 else b""

        env = dict(os.environ)
        env["REQUEST_METHOD"] = self.command
        env["PATH_INFO"] = path_info
        env["QUERY_STRING"] = query_string
        env["CONTENT_LENGTH"] = str(len(body))
        env["HOSPITAL_DB_PATH"] = DB_PATH

        if "Cookie" in self.headers:
            env["HTTP_COOKIE"] = self.headers["Cookie"]
        if "Authorization" in self.headers:
            env["HTTP_AUTHORIZATION"] = self.headers["Authorization"]
        if "X-Session-Token" in self.headers:
            env["HTTP_X_SESSION_TOKEN"] = self.headers["X-Session-Token"]
        if "Content-Type" in self.headers:
            env["CONTENT_TYPE"] = self.headers["Content-Type"]

        try:
            proc = subprocess.Popen(
                [CGI_EXE],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                cwd=BASE_DIR,
                env=env
            )
            stdout, stderr = proc.communicate(input=body, timeout=15)
            if stderr:
                sys.stderr.write(stderr.decode('utf-8', errors='ignore'))

            # Parse CGI headers and body
            header_end = stdout.find(b"\r\n\r\n")
            status_code = 200
            headers = []

            if header_end != -1:
                header_part = stdout[:header_end].decode("utf-8", errors="ignore")
                body_part = stdout[header_end + 4:]
                for line in header_part.split("\r\n"):
                    if not line:
                        continue
                    if line.lower().startswith("status:"):
                        try:
                            status_code = int(line.split(":", 1)[1].strip().split()[0])
                        except:
                            pass
                    elif ":" in line:
                        key, val = line.split(":", 1)
                        headers.append((key.strip(), val.strip()))
            else:
                body_part = stdout

            self.send_response(status_code)
            for k, v in headers:
                self.send_header(k, v)
            if "Content-Length" not in [k for k, _ in headers]:
                self.send_header("Content-Length", str(len(body_part)))
            self.end_headers()
            self.wfile.write(body_part)

        except Exception as e:
            self.send_response(500)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            err_msg = f'{{"success": false, "message": "CGI execution error: {str(e)}"}}'
            self.wfile.write(err_msg.encode("utf-8"))

class ReusableTCPServer(socketserver.TCPServer):
    allow_reuse_address = True

if __name__ == "__main__":
    if not os.path.exists(CGI_EXE):
        print(f"Error: Compiled C CGI binary not found at {CGI_EXE}")
        print("Please run 'make' or compile the C backend first.")
        sys.exit(1)

    print(f"============================================================")
    print(f"  HOSPITAL MANAGEMENT SYSTEM - C BACKEND WEB SERVER")
    print(f"============================================================")
    print(f"  Frontend URL : http://localhost:{PORT}")
    print(f"  C Backend CGI: {CGI_EXE}")
    print(f"  Database     : {DB_PATH}")
    print(f"============================================================")
    print(f"  Press Ctrl+C to stop the server.")
    print(f"============================================================")

    with ReusableTCPServer(("", PORT), HospitalHTTPRequestHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server.")

