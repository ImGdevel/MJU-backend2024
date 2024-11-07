from flask import Flask
from flask import request
from flask import make_response

app = Flask(__name__)

# 계산
def calculate(arg1, op, arg2):
    if op == '+':
        return arg1 + arg2
    elif op == '-':
        return arg1 - arg2
    elif op == '*':
        return arg1 * arg2
    else:
        return None

@app.route('/<arg1>/<op>/<arg2>', methods=['GET'])
def get_calculate(arg1, op, arg2):
    arg1 = int(arg1)
    arg2 = int(arg2)
    result = calculate(arg1, op, arg2)
    if result is None:
        return 400
    
    resp = make_response(str(result), 200)
    return resp

@app.route('/', methods=['POST'])
def post_calculate():
    data = request.get_json()
    if not data or 'arg1' not in data or 'op' not in data or 'arg2' not in data:
        return 400
    
    arg1 = data['arg1']
    op = data['op']
    arg2 = data['arg2']

    result = calculate(arg1, op, arg2)
    if result is None:
        return 400
    
    resp = make_response(str(result), 200)
    return resp

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=10114)