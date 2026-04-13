from flask import Flask, jsonify, request, send_from_directory
import os
import shutil

app = Flask(__name__)

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOWER = os.path.join(BASE, 'lower')
UPPER = os.path.join(BASE, 'upper')
MOUNT = os.path.join(BASE, 'mount')

def get_layer_files(path):
    files = []
    if not os.path.exists(path):
        return files
    for f in os.listdir(path):
        full = os.path.join(path, f)
        files.append({
            'name': f,
            'is_whiteout': f.startswith('.wh_'),
            'size': os.path.getsize(full),
            'is_dir': os.path.isdir(full)
        })
    return files

@app.route('/')
def index():
    return send_from_directory('.', 'index.html')

@app.route('/api/state')
def state():
    return jsonify({
        'lower': get_layer_files(LOWER),
        'upper': get_layer_files(UPPER),
        'mount': get_layer_files(MOUNT),
    })

@app.route('/api/read', methods=['POST'])
def read_file():
    data = request.json
    fname = data.get('filename')
    path = os.path.join(MOUNT, fname)
    if not os.path.exists(path):
        return jsonify({'error': f'{fname} not found in mount'}), 404
    with open(path, 'r') as f:
        content = f.read()
    return jsonify({'filename': fname, 'content': content})

@app.route('/api/write', methods=['POST'])
def write_file():
    data = request.json
    fname = data.get('filename')
    content = data.get('content', '')
    path = os.path.join(MOUNT, fname)
    if not os.path.ismount(MOUNT) and not os.path.exists(MOUNT):
        return jsonify({'error': 'mount not active'}), 400
    with open(path, 'w') as f:
        f.write(content)
    return jsonify({'message': f'{fname} written to mount'})

@app.route('/api/delete', methods=['POST'])
def delete_file():
    data = request.json
    fname = data.get('filename')
    path = os.path.join(MOUNT, fname)
    if not os.path.exists(path):
        return jsonify({'error': f'{fname} not found'}), 404
    os.unlink(path)
    return jsonify({'message': f'{fname} deleted'})

@app.route('/api/add_lower', methods=['POST'])
def add_lower():
    data = request.json
    fname = data.get('filename')
    content = data.get('content', '')
    path = os.path.join(LOWER, fname)
    with open(path, 'w') as f:
        f.write(content)
    return jsonify({'message': f'{fname} added to lower'})

if __name__ == '__main__':
    app.run(debug=True, port=5000)
