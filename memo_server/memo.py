from http import HTTPStatus
import random
import requests
import json
import urllib
import string

from flask import abort, Flask, make_response, render_template, Response, redirect, request

app = Flask(__name__)


naver_client_id = 'Fhx9wb8Fjsn3IkJguo9r'
naver_client_secret = 'ADPhH5Bppy'
naver_redirect_uri = 'http://mjubackend.duckdns.org/auth'

user_id_map = {}
temp_db = {}
temp_db_memo = {}

@app.route('/')
def home():
    # HTTP 세션 쿠키를 통해 이전에 로그인 한 적이 있는지를 확인한다.
    # 이 부분이 동작하기 위해서는 OAuth 에서 access token 을 얻어낸 뒤
    # user profile REST api 를 통해 유저 정보를 얻어낸 뒤 'userId' 라는 cookie 를 지정해야 된다.
    # (참고: 아래 onOAuthAuthorizationCodeRedirected() 마지막 부분 response.set_cookie('userId', user_id) 참고)
    userId = request.cookies.get('userId', default=None)
    name = None
    ####################################################
    # TODO: 아래 부분을 채워 넣으시오.
    #       userId 로부터 DB 에서 사용자 이름을 얻어오는 코드를 여기에 작성해야 함
    #임시 작성
    if not userId is None and userId in user_id_map:
        userId = user_id_map[userId]
        name = temp_db[userId]
    ####################################################


    # 이제 클라에게 전송해 줄 index.html 을 생성한다.
    # template 로부터 받아와서 name 변수 값만 교체해준다.
    return render_template('index.html', name=name)


# 로그인 버튼을 누른 경우 이 API 를 호출한다.
# 브라우저가 호출할 URL 을 index.html 에 하드코딩하지 않고,
# 아래처럼 서버가 주는 URL 로 redirect 하는 것으로 처리한다.
# 이는 CORS (Cross-origin Resource Sharing) 처리에 도움이 되기도 한다.
#
# 주의! 아래 API 는 잘 동작하기 때문에 손대지 말 것
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



# 아래는 Authorization code 가 발급된 뒤 Redirect URI 를 통해 호출된다.
@app.route('/auth')
def onOAuthAuthorizationCodeRedirected():
    # TODO: 아래 1 ~ 4 를 채워 넣으시오.

    # 1. redirect uri 를 호출한 request 로부터 authorization code 와 state 정보를 얻어낸다.
    authorization_code = request.args.get('code')
    state = request.args.get('state')

    if not authorization_code or not state:
        return "Invalid request: Missing code or state", 400

    # 2. authorization code 로부터 access token 을 얻어내는 네이버 API 를 호출한다.
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

    # 3. 얻어낸 access token 을 이용해서 프로필 정보를 반환하는 API 를 호출하고,
    #    유저의 고유 식별 번호를 얻어낸다.
    api_url = 'https://openapi.naver.com/v1/nid/me'
    headers = {
        "Authorization": f"Bearer {access_token}"
    }
    response_user_profile = requests.get(api_url, headers=headers)
    if response_user_profile.status_code != 200:
        return "Failed to fetch user profile", 500

    # 4. 얻어낸 user id 와 name 을 DB 에 저장한다.
    profile_data = response_user_profile.json()
    user_id = profile_data['response']['id']
    user_name = profile_data['response']['name']
    
    if not user_id or not user_name:
        return "User ID or name not found", 500
    
    save_user_to_db(user_id, user_name)

    # 5. 첫 페이지로 redirect 하는데 로그인 쿠키를 설정하고 보내준다.
    #    user_id 쿠키는 "dkmoon" 처럼 정말 user id 를 바로 집어 넣는 것이 아니다.
    #    그렇게 바로 user id 를 보낼 경우 정보가 노출되기 때문이다.
    #    대신 user_id cookie map 을 두고, random string -> user_id 형태로 맵핑을 관리한다.
    #      예: user_id_map = {}
    #          key = random string 으로 얻어낸 a1f22bc347ba3 이런 문자열
    #          user_id_map[key] = real_user_id
    #          user_id = key

    random_key = ''.join(random.choices(string.ascii_letters + string.digits, k=16))
    user_id_map[random_key] = user_id
    user_id = random_key

    response = redirect('/')
    response.set_cookie('userId', user_id)
    return response


def save_user_to_db(user_id, user_name):
    print("DB에 유저 저장", user_id, user_name)
    temp_db[user_id] = user_name



memo_list = [
    {"text" : "test"},
    {"text" : "bold"}
]

@app.route('/memo', methods=['GET'])
def get_memos():
    # 로그인이 안되어 있다면 로그인 하도록 첫 페이지로 redirect 해준다.
    userId = request.cookies.get('userId', default=None)
    if not userId:
        return redirect('/')

    # TODO: DB 에서 해당 userId 의 메모들을 읽어오도록 아래를 수정한다.
    result = memo_list

    # memos라는 키 값으로 메모 목록 보내주기
    return {'memos': result}


@app.route('/memo', methods=['POST'])
def post_new_memo():
    # 로그인이 안되어 있다면 로그인 하도록 첫 페이지로 redirect 해준다.
    userId = request.cookies.get('userId', default=None)
    if not userId:
        return redirect('/')

    # 클라이언트로부터 JSON 을 받았어야 한다.
    if not request.is_json:
        abort(HTTPStatus.BAD_REQUEST)

    # TODO: 클라이언트로부터 받은 JSON 에서 메모 내용을 추출한 후 DB에 userId 의 메모로 추가한다.
    data = request.get_json()
    text = data.get('text')

    if not text:
        abort(HTTPStatus.BAD_REQUEST)

    memo_list.append({
        "text": text
    })

    return '', HTTPStatus.OK


if __name__ == '__main__':
    app.run('0.0.0.0', port=10114, debug=True)
