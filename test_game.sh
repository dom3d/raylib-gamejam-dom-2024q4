#!/usr/bin/env bash
open http://localhost:8000/
python3 -m http.server -d ./src/ 8000
