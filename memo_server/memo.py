from http import HTTPStatus
import random
import requests
import json
import urllib
import string

from flask import abort, Flask, make_response, render_template, Response, redirect, request
from flask_sqlalchemy import SQLAlchemy
import pymysql
import redis

app = Flask(__name__)

app.config['SQLALCHEMY_DATABASE_URI'] = 'mysql+pymysql://root:1234@172.31.41.207:3306/memo'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
app.config['SQLALCHEMY_ECHO'] = False

db = SQLAlchemy(app)
rd = redis.StrictRedis(host='172.31.41.207', port=6379, db=0, decode_responses=True)

naver_client_id = 'Fhx9wb8Fjsn3IkJguo9r'
naver_client_secret = 'ADPhH5Bppy'
naver_redirect_uri = 'http://memo-server-load-balancer-823271627.ap-northeast-2.elb.amazonaws.com/memo/auth'

class User(db.Model):
    __tablename__ = 'user'
    id = db.Column(db.String(255), primary_key=True)
    name = db.Column(db.String(255), nullable=False)

class Memo(db.Model):
    __tablename__ = 'memo'
    id = db.Column(db.BigInteger, primary_key=True)
    user_id = db.Column(db.String(255), db.ForeignKey('user.id'), nullable=False)
    text = db.Column(db.String(1000), nullable=False)


@app.route('/')
def home():

    userId = request.cookies.get('userId', default=None)
    name = None

    if not userId is None:
        userId = rd.get(userId)
        user = User.query.get(userId)
        if user:
            name = user.name

    return render_template('index.html', name=name)


@app.route('/login')
def onLogin():
    params={
            'response_type': 'code',
            'client_id': naver_client_id,
            'redirect_uri': naver_redirect_uri,
            'state': random.randint(0, 10000)
        }
    urlencoded = urllib.parse.urlencode(params)
    url = f'https://nid.naver.com/oauth2.0/authorize?{urlencoded}'
    return redirect(url)



@app.route('/auth')
def onOAuthAuthorizationCodeRedirected():

    authorization_code = request.args.get('code')
    state = request.args.get('state')

    if not authorization_code or not state:
        return "Invalid request: Missing code or state", 400

    token_url = "https://nid.naver.com/oauth2.0/token"
    token_params = {
        'grant_type': 'authorization_code',
        'client_id': naver_client_id,
        'client_secret': naver_client_secret,
        'code': authorization_code,
        'state': state
    }
    response_access_token = requests.post(token_url, data=token_params)
    if response_access_token.status_code != 200:
        return "Failed to obtain access token", 500

    access_token = response_access_token.json().get('access_token')

    if not access_token:
        return "Access token not found", 500

    api_url = 'https://openapi.naver.com/v1/nid/me'
    headers = {
        "Authorization": f"Bearer {access_token}"
    }
    response_user_profile = requests.get(api_url, headers=headers)
    if response_user_profile.status_code != 200:
        return "Failed to fetch user profile", 500

    profile_data = response_user_profile.json()
    user_id = profile_data['response']['id']
    user_name = profile_data['response']['name']

    if not user_id or not user_name:
        return "User ID or name not found", 500

    try:
        user = User.query.get(user_id)
        if not user:
            user = User(id=user_id, name=user_name)
            db.session.add(user)
            db.session.commit()
    except:
        print("[Error] Database Not Connection")
        return "Database Not Connection", 500

    random_key = ''.join(random.choices(string.ascii_letters + string.digits, k=16))
    rd.set(random_key, user_id)

    print("[LOG] login | user id:", rd.get(random_key), "random key", random_key )



    response = redirect('/memo/')
    response.set_cookie('userId', random_key)
    return response


@app.route('/memo', methods=['GET'])
def get_memos():
    userId = request.cookies.get('userId', default=None)
    if not userId:
        return redirect('/')

    user_db_id = rd.get(userId)

    memos = Memo.query.filter_by(user_id=user_db_id).all()
    result = [{"id": memo.id, "text": memo.text} for memo in memos]

    return {'memos': result}


@app.route('/memo', methods=['POST'])
def post_new_memo():
    userId = request.cookies.get('userId', default=None)
    if not userId:
        return redirect('/')

    if not request.is_json:
        abort(HTTPStatus.BAD_REQUEST)

    data = request.get_json()
    text = data.get('text')

    print("[LOG] memo write | text:", text)

    if not text:
        abort(HTTPStatus.BAD_REQUEST)

    user_id = rd.get(userId)
    new_memo = Memo(user_id=user_id, text=text)
    db.session.add(new_memo)
    db.session.commit()

    return '', HTTPStatus.OK