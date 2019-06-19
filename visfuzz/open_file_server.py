#!/usr/bin/env python3
from http.server import HTTPServer, SimpleHTTPRequestHandler, test, BaseHTTPRequestHandler
from os import curdir
import os
import random
import sys
import logging
import json

MIN_PATH = '/out/min_queue'


class CORSRequestHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        print(self.path)
        if self.path == '/out/queue/seeds':
            dic = {}
            counter = 0
            files = os.listdir(curdir + MIN_PATH)
            for file in files:
                if counter == 3:
                    break
                if not os.path.isdir(file):
                    with open(curdir + MIN_PATH + "/" + file, 'rb') as f:
                        try:
                            file_context = f.read()
                            dic[counter] = str(file_context)
                            counter += 1
                        except:
                            print(file_context)
            print(dic)
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(dic).encode())
            # self.send_response(200)
        else:
            super(CORSRequestHandler, self).do_GET()

    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        body = self.rfile.read(content_length)
        path = curdir + '/out/queue/' + ''.join(
            [str(random.randint(0, 9)) for _ in range(10)])
        with open(path, 'w') as f:
            f.write(body.decode())
        self.send_response(200)

    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        SimpleHTTPRequestHandler.end_headers(self)


if __name__ == '__main__':
    test(CORSRequestHandler,
         HTTPServer,
         port=int(sys.argv[1]) if len(sys.argv) > 1 else 8000)
