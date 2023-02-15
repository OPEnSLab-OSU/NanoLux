from fastapi import FastAPI
from fastapi.exceptions import RequestValidationError
from fastapi.middleware.cors import CORSMiddleware
from starlette import status
from starlette.requests import Request
from starlette.responses import JSONResponse

from data_model import Settings, Pattern, Control

app = FastAPI()

origins = [
    "http://localhost:8080",
    "http://localhost:8000",
    "http://localhost",
    "https://audioluxecampus.azurewebsites.net/",
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"]
)

current_pattern = Pattern.trail
settings = Settings(
    noise=10,
    compression=90,
    loFreqHue=55,
    hiFreqHue=200,
    ledCount=50
)


history = []

@app.exception_handler(RequestValidationError)
async def validation_exception_handler(request: Request, exc: RequestValidationError):
    exc_str = f'{exc}'.replace('\n', ' ').replace('   ', ' ')
    print(f'{exc}\n{request}')
    # logger.error(request, exc_str)
    content = {'status_code': 10422, 'message': exc_str, 'data': None}
    return JSONResponse(content=content, status_code=status.HTTP_422_UNPROCESSABLE_ENTITY)


@app.get("/api/settings")
def get_settings():
    history.append(f"Retrieved settings: {settings}\n")
    return settings


@app.put("/api/settings")
def put_settings(new_settings: Settings):
    global settings
    settings = new_settings.copy()
    history.append(f"Saved settings: {settings}\n")
    return {"message": "Settings saved."}


@app.get("/api/patterns")
def get_pattern_list():
    history.append(f"Retrieved pattern list: {[p.name for p in Pattern]}\n")
    return [pattern.value for pattern in Pattern]


@app.get("/api/pattern")
def get_current_pattern():
    history.append(f"Retrieved current pattern: {current_pattern}\n")
    return current_pattern


@app.put("/api/pattern")
def put_current_pattern(request: Control):
    global current_pattern
    current_pattern = request.pattern
    print(f"New pattern: {current_pattern}")
    history.append(f"Saved pattern: {current_pattern}\n")
    return {"message": "Pattern saved."}


@app.get("/api/history")
def get_history():
    global history
    saved_history = history.copy()
    history = []
    return saved_history