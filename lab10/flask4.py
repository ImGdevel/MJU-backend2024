from flask import Flask
from flask import make_response

app = Flask(__name__)

@app.route('/<greeting>/<name>')
def greet(greeting, name):
    return make_response(f'{greeting}, {name}!', 400)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=10114)
