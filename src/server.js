
import express from "express";
import mongoose from "mongoose";
import { nanoid } from "nanoid";
import dotenv from "dotenv";

dotenv.config();

const app = express();

app.use(express.json());

// 1. Connect to MongoDB
mongoose
  .connect(process.env.MONGO_URI)
  .then(() => console.log("MongoDB connected"))
  .catch((err) => console.error("MongoDB error:", err.message));

// 2. Define URL schema
const urlSchema = new mongoose.Schema(
  {
    originalUrl: {
      type: String,
      required: true,
    },
    shortCode: {
      type: String,
      required: true,
      unique: true,
    },
    clicks: {
      type: Number,
      default: 0,
    },
  },
  { timestamps: true }
);

const URL = mongoose.model("URL", urlSchema);

// 3. Create a short URL
app.post("/api/shorten", async (req, res) => {
  try {
    const { originalUrl } = req.body;

    if (!originalUrl) {
      return res.status(400).json({
        error: "Original URL is required",
      });
    }

    let parsedUrl;

    try {
      parsedUrl = new URL(originalUrl);
    } catch {
      return res.status(400).json({
        error: "Invalid URL",
      });
    }

    if (!["http:", "https:"].includes(parsedUrl.protocol)) {
      return res.status(400).json({
        error: "Only HTTP and HTTPS URLs are supported",
      });
    }

    const shortCode = nanoid(7);

    const savedUrl = await URL.create({
      originalUrl,
      shortCode,
    });

    res.status(201).json({
      message: "Short URL created successfully",
      originalUrl: savedUrl.originalUrl,
      shortUrl: `${process.env.BASE_URL}/${savedUrl.shortCode}`,
      shortCode: savedUrl.shortCode,
    });
  } catch (error) {
    res.status(500).json({
      error: "Failed to create short URL",
    });
  }
});

// 4. Redirect to original URL
app.get("/:shortCode", async (req, res) => {
  try {
    const url = await URL.findOne({
      shortCode: req.params.shortCode,
    });

    if (!url) {
      return res.status(404).json({
        error: "Short URL not found",
      });
    }

    url.clicks += 1;
    await url.save();

    return res.redirect(302, url.originalUrl);
  } catch (error) {
    res.status(500).json({
      error: "Server error",
    });
  }
});


app.get("/health", (req, res) => {
  res.status(200).json({
    status: "OK",
    message: "URL Shortener API is running",
    timestamp: new Date().toISOString()
  });
});


// 5. Get URL analytics
app.get("/api/stats/:shortCode", async (req, res) => {
  try {
    const url = await URL.findOne({
      shortCode: req.params.shortCode,
    });

    if (!url) {
      return res.status(404).json({
        error: "Short URL not found",
      });
    }

    res.json({
      originalUrl: url.originalUrl,
      shortCode: url.shortCode,
      clicks: url.clicks,
      createdAt: url.createdAt,
    });
  } catch (error) {
    res.status(500).json({
      error: "Failed to fetch analytics",
    });
  }
});

// 6. Start server
const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
  console.log(`Server running on port ${PORT}`);
});
