from flask import Flask

app = Flask(__name__)

@app.route('/')
def hello_world():
    return 'Hello, World!'
@app.route('/bad')
def hello_world1():
    return 'Bad World!'
@app.route('/good')
def hello_world2():
    return 'Good World!'
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=10114)
