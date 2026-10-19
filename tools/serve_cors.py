#!/usr/bin/env python3
import argparse
import functools
import http.server
import mimetypes
import os
import socketserver


DEFAULT_HEADERS = (
    ("Cross-Origin-Opener-Policy", "same-origin"),
    ("Cross-Origin-Embedder-Policy", "require-corp"),
    ("Cross-Origin-Resource-Policy", "cross-origin"),
    ("Access-Control-Allow-Origin", "*"),
    ("Access-Control-Allow-Methods", "GET, HEAD, OPTIONS"),
    ("Access-Control-Allow-Headers", "*"),
    ("Cache-Control", "no-store"),
)


class ThreadingHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    allow_reuse_address = True
    daemon_threads = True


class CorsRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        for name, value in DEFAULT_HEADERS:
            self.send_header(name, value)
        http.server.SimpleHTTPRequestHandler.end_headers(self)

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()


def parse_args():
    parser = argparse.ArgumentParser(description="Serve a directory with CORS and cross-origin isolation headers.")
    parser.add_argument("directory", help="Directory to serve.")
    parser.add_argument("--host", default="127.0.0.1", help="Host/interface to bind. Use 0.0.0.0 for LAN access.")
    parser.add_argument("--port", type=int, default=8008, help="Port to bind.")
    return parser.parse_args()


def main():
    args = parse_args()
    directory = os.path.abspath(args.directory)
    if not os.path.isdir(directory):
        raise SystemExit(f"Not a directory: {directory}")

    mimetypes.add_type("application/wasm", ".wasm")
    mimetypes.add_type("application/javascript", ".js")

    handler = functools.partial(CorsRequestHandler, directory=directory)
    with ThreadingHTTPServer((args.host, args.port), handler) as server:
        print(f"Serving {directory}")
        print(f"URL: http://{args.host}:{args.port}/")
        print("Headers: COOP=same-origin, COEP=require-corp, CORS=*")
        server.serve_forever()


if __name__ == "__main__":
    main()
