import time
import uuid
import sys

for i in range(5):
    print("hello testing")
    print("Session:", uuid.uuid4())
    sys.stdout.flush()
    time.sleep(1)
print("EOL")
