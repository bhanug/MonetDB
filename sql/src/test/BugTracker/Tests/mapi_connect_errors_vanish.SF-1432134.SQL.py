import sys
from MonetDBtesting import process

c = process.client('sql', user = 'invalid', passwd = 'invalid',
                   stdout = process.PIPE, stderr = process.PIPE)
out, err = c.communicate()
sys.stdout.write(out)
sys.stderr.write(err)
